#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <filesystem>
#include <iostream>
#include <thread>

#include "json_loader.h"
#include "logging_request_handler.h"
#include "request_handler.h"
#include "sdk.h"
#include "state_manager.h"
#include "ticker.h"

using namespace std::literals;
namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace po = boost::program_options;
namespace fs = std::filesystem;

namespace {

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
  n = std::max(1u, n);
  std::vector<std::jthread> workers;
  workers.reserve(n - 1);
  while (--n) {
    workers.emplace_back(fn);
  }
  fn();
}

void PrintHelp(const po::options_description& desc) {
  std::cout << "Allowed options:\n" << desc << std::endl;
}

struct Args {
  std::string config_file;
  std::string www_root;
  std::optional<int> tick_period_ms;
  std::optional<std::string> state_file;
  std::optional<int> save_state_period_ms;
  bool randomize_spawn = false;
  bool help = false;
};

std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
  po::options_description desc("Allowed options");
  Args args;

  desc.add_options()("help,h", po::bool_switch(&args.help),
                     "produce help message")("tick-period,t", po::value<int>(),
                                             "set tick period in milliseconds")(
      "config-file,c", po::value<std::string>(&args.config_file),
      "set config file path")("www-root,w",
                              po::value<std::string>(&args.www_root),
                              "set static files root")(
      "randomize-spawn-points", po::bool_switch(&args.randomize_spawn),
      "spawn dogs at random positions")(
      "state-file", po::value<std::string>(),
      "path to state file for saving/loading")(
      "save-state-period", po::value<int>(),
      "game time period in milliseconds for auto-saving");

  po::positional_options_description positional_desc;
  positional_desc.add("config-file", 1);
  positional_desc.add("www-root", 1);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(argc, argv)
                  .options(desc)
                  .positional(positional_desc)
                  .run(),
              vm);
    po::notify(vm);
  } catch (const std::exception& e) {
    std::cerr << "Error parsing command line: " << e.what() << std::endl;
    PrintHelp(desc);
    return std::nullopt;
  }

  if (args.help) {
    PrintHelp(desc);
    return std::nullopt;
  }

  if (args.config_file.empty() || args.www_root.empty()) {
    std::cerr << "Error: config-file and www-root are required" << std::endl;
    PrintHelp(desc);
    return std::nullopt;
  }

  if (!fs::exists(args.config_file)) {
    std::cerr << "Error: config file not found: " << args.config_file
              << std::endl;
    return std::nullopt;
  }

  if (!fs::exists(args.www_root)) {
    std::cerr << "Error: www-root directory not found: " << args.www_root
              << std::endl;
    return std::nullopt;
  }

  if (vm.count("tick-period")) {
    args.tick_period_ms = vm["tick-period"].as<int>();
    if (args.tick_period_ms.value() <= 0) {
      std::cerr << "Error: tick period must be positive" << std::endl;
      return std::nullopt;
    }
  }

  if (vm.count("state-file")) {
    args.state_file = vm["state-file"].as<std::string>();
  }

  if (vm.count("save-state-period")) {
    args.save_state_period_ms = vm["save-state-period"].as<int>();
    if (args.save_state_period_ms.value() <= 0) {
      std::cerr << "Error: save-state-period must be positive" << std::endl;
      return std::nullopt;
    }
    if (!args.state_file.has_value()) {
      std::cerr << "Warning: --save-state-period ignored without --state-file"
                << std::endl;
      args.save_state_period_ms.reset();
    }
  }

  return args;
}

}  // namespace

int main(int argc, const char* argv[]) {
  logging_handler::InitLogging();

  try {
    auto args_opt = ParseCommandLine(argc, argv);
    if (!args_opt.has_value()) {
      return EXIT_SUCCESS;
    }

    const auto& args = args_opt.value();

    // Выводим информацию о запуске
    std::cout << "Starting server with:" << std::endl;
    std::cout << "  Config file: " << args.config_file << std::endl;
    std::cout << "  Static root: " << args.www_root << std::endl;
    if (args.tick_period_ms.has_value()) {
      std::cout << "  Tick period: " << args.tick_period_ms.value() << " ms"
                << std::endl;
    } else {
      std::cout << "  Tick mode: manual (API)" << std::endl;
    }
    std::cout << "  Randomize spawn: "
              << (args.randomize_spawn ? "enabled" : "disabled") << std::endl;
    if (args.state_file.has_value()) {
      std::cout << "  State file: " << args.state_file.value() << std::endl;
    } else {
      std::cout << "  State persistence: disabled" << std::endl;
    }
    if (args.save_state_period_ms.has_value()) {
      std::cout << "  Auto-save period: " << args.save_state_period_ms.value()
                << " ms (game time)" << std::endl;
    }

    model::Game game = json_loader::LoadGame(args.config_file);
    std::cout << "Game loaded successfully" << std::endl;

    // Инициализируем StateManager
    state_manager::StateManager state_manager;

    // Создаем Players ДО загрузки состояния
    model::Players players(&game, args.randomize_spawn);
    bool state_loaded = false;

    if (args.state_file.has_value()) {
      try {
        state_loaded =
            state_manager.Load(game, players, args.state_file.value());
        if (state_loaded) {
          std::cout << "State restored from " << args.state_file.value()
                    << std::endl;
        } else {
          std::cout << "State file not found, starting fresh" << std::endl;
        }
      } catch (const std::exception& e) {
        logging_handler::LogServerExit(EXIT_FAILURE, e.what());
        std::cerr << "Error restoring state: " << e.what() << std::endl;
        return EXIT_FAILURE;
      }
    }

    const unsigned num_threads = std::thread::hardware_concurrency();
    net::io_context ioc(num_threads);

    auto api_strand = net::make_strand(ioc);

    // Передаем players по ссылке в RequestHandler
    auto handler = std::make_shared<http_handler::RequestHandler>(
        game, players, args.www_root, args.config_file, args.randomize_spawn,
        api_strand);

    std::shared_ptr<Ticker> ticker;
    if (args.tick_period_ms.has_value()) {
      handler->SetTickEnabled(true);
      ticker = std::make_shared<Ticker>(
          api_strand, std::chrono::milliseconds(args.tick_period_ms.value()),
          [handler, &game, &players, &state_manager,
           &args](std::chrono::milliseconds delta) {
            // Обновляем игровое время
            game.AddGameTime(delta);

            // Выполняем тик игры
            handler->Tick(delta);

            // ===== АВТОСОХРАНЕНИЕ ПОСЛЕ КАЖДОГО ТИКА =====
            // Это гарантирует сохранение даже при SIGKILL
            if (args.state_file.has_value()) {
              try {
                state_manager.Save(game, players, args.state_file.value());
                std::cout << "State saved after tick" << std::endl;
              } catch (const std::exception& e) {
                std::cerr << "Error saving state after tick: " << e.what()
                          << std::endl;
              }
            }
          });
      ticker->Start();
      std::cout << "Ticker started with period " << args.tick_period_ms.value()
                << " ms" << std::endl;
    }

    net::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&ioc, ticker, &game, &players, &state_manager, &args](
                           const boost::system::error_code&, int) {
      logging_handler::LogServerExit(0);
      std::cout << "Shutting down server..." << std::endl;

      if (ticker) {
        ticker->Stop();
      }

      // Сохраняем состояние при завершении, если указан файл состояния
      if (args.state_file.has_value()) {
        try {
          state_manager.Save(game, players, args.state_file.value());
          std::cout << "State saved on shutdown to " << args.state_file.value()
                    << std::endl;
        } catch (const std::exception& e) {
          std::cerr << "Error saving state on shutdown: " << e.what()
                    << std::endl;
        }
      }

      ioc.stop();
    });

    const std::string address = "0.0.0.0";
    const unsigned short port = 8080;
    tcp::endpoint endpoint{net::ip::make_address(address), port};

    logging_handler::LogServerStart(address, port);
    std::cout << "Server listening on " << address << ":" << port << std::endl;

    http_server::ServeHttp(
        ioc, endpoint,
        [handler](auto&& req, const std::string& client_ip, auto&& send) {
          net::dispatch(
              handler->GetStrand(),
              [handler, req = std::forward<decltype(req)>(req), client_ip,
               send = std::forward<decltype(send)>(send)]() mutable {
                (*handler)(std::forward<decltype(req)>(req), client_ip,
                           std::forward<decltype(send)>(send));
              });
        });

    RunWorkers(std::max(1u, num_threads), [&ioc] { ioc.run(); });

    std::cout << "Server stopped." << std::endl;

  } catch (const std::exception& ex) {
    logging_handler::LogServerExit(EXIT_FAILURE, ex.what());
    std::cerr << "FATAL ERROR: " << ex.what() << std::endl;
    return EXIT_FAILURE;
  }

  return 0;
}