#pragma once
#include <boost/asio/strand.hpp>
#include <boost/beast/http/file_body.hpp>
#include <boost/json.hpp>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <regex>
#include <sstream>

#include "api_handlers.h"
#include "game_state.h"
#include "http_server.h"
#include "json_loader.h"
#include "logging_request_handler.h"
#include "mime_types.h"
#include "model.h"
#include "players.h"
#include "record_manager.h"
#include "response_helpers.h"
#include "serializers.h"
#include "state_manager.h"
#include "url_utils.h"
#include "tagged.h"

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;

namespace endpoints {
constexpr std::string_view API_PREFIX = "/api/";
constexpr std::string_view GAME_TICK = "/api/v1/game/tick";
constexpr std::string_view MAPS = "/api/v1/maps";
constexpr std::string_view GAME_JOIN = "/api/v1/game/join";
constexpr std::string_view GAME_PLAYERS = "/api/v1/game/players";
constexpr std::string_view GAME_STATE = "/api/v1/game/state";
constexpr std::string_view PLAYER_ACTION = "/api/v1/game/player/action";
constexpr std::string_view GAME_RECORDS = "/api/v1/game/records";
}  // namespace endpoints

namespace headers {
constexpr std::string_view CONTENT_TYPE = "Content-Type";
constexpr std::string_view CACHE_CONTROL = "Cache-Control";
constexpr std::string_view AUTHORIZATION = "Authorization";
constexpr std::string_view ALLOW = "Allow";
}  // namespace headers

class RequestHandler {
 public:
  explicit RequestHandler(model::Game& game, model::Players& players,
                          state_manager::StateManager& state_manager,
                          const std::optional<std::string>& state_file,
                          const std::filesystem::path& static_dir,
                          const std::filesystem::path& config_path,
                          bool randomize_spawn,
                          net::strand<net::io_context::executor_type> strand,
                          records::RecordManager& record_manager)
      : game_{game},
        players_{players},
        state_manager_{state_manager},
        state_file_{state_file},
        static_dir_{static_dir},
        config_path_{config_path},
        strand_{strand},
        tick_enabled_{false},
        record_manager_{record_manager},
        inactivity_info_{},                          // <-- добавить
        players_to_retire_{},                        // <-- добавить
        retirement_time_{std::chrono::minutes(1)} {  // <-- добавить
    LoadConfig();
  }

  RequestHandler(const RequestHandler&) = delete;
  RequestHandler& operator=(const RequestHandler&) = delete;

  void Tick(std::chrono::milliseconds delta_time) {
    auto current_time = game_.GetGameTime();
    current_time += delta_time;
    game_.SetGameTime(current_time);

    // Проверяем бездействие собак и отправляем на покой
    CheckInactiveDogs(current_time);

    // Двигаем собак и собираем предметы
    for (auto& map : game_.GetMaps()) {
      game_state::UpdateDogsPositionAndGather(
          map, delta_time, game_.GetLostObjectsMutable(), current_time,
          retirement_time_, inactivity_info_, players_to_retire_);
    }

    // Обрабатываем уход на покой
    for (const auto& player_ptr : players_to_retire_) {
      RetirePlayer(player_ptr);
    }
    players_to_retire_.clear();

    game_.UpdateTime(delta_time);

    // Сохраняем состояние после тика
    SaveState();
  }

  void SetTickEnabled(bool enabled) { tick_enabled_ = enabled; }

  const net::strand<net::io_context::executor_type>& GetStrand() const {
    return strand_;
  }

  void SaveState() {
    if (state_file_.has_value()) {
      try {
        state_manager_.Save(game_, players_, state_file_.value());
      } catch (const std::exception& e) {
        std::cerr << "Error saving state: " << e.what() << std::endl;
      }
    }
  }

  template <typename Body, typename Allocator, typename Send>
  void operator()(http::request<Body, http::basic_fields<Allocator>>&& req,
                  const std::string& client_ip, Send&& send) {
    std::string target(req.target());
    std::string method(req.method_string());
    logging_handler::LogRequest(client_ip, target, method);

    auto start_time = std::chrono::steady_clock::now();

    auto logging_send = [this, send = std::forward<Send>(send), client_ip,
                         start_time](auto&& response) mutable {
      auto end_time = std::chrono::steady_clock::now();
      auto response_time =
          std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                start_time);

      int status_code = response.result_int();
      std::string content_type = "text/plain";
      auto it = response.find(http::field::content_type);
      if (it != response.end()) {
        content_type = std::string(it->value());
      }

      logging_handler::LogResponse(client_ip, response_time, status_code,
                                   content_type);
      send(std::forward<decltype(response)>(response));
    };

    if (target.rfind(endpoints::API_PREFIX.data(), 0) == 0) {
      HandleApiRequest(std::move(req), std::move(logging_send));
      return;
    }

    HandleStaticFile(std::move(req), std::move(logging_send));
  }

 private:
  model::Game& game_;
  model::Players& players_;
  state_manager::StateManager& state_manager_;
  std::optional<std::string> state_file_;
  std::filesystem::path static_dir_;
  std::filesystem::path config_path_;
  boost::json::value config_json_;
  net::strand<net::io_context::executor_type> strand_;
  bool tick_enabled_;
  records::RecordManager& record_manager_;  // <-- Добавлен RecordManager

  // Для отслеживания бездействия
  std::unordered_map<model::Dog::Id, game_state::DogInactivityInfo,
                     util::TaggedHasher<model::Dog::Id>>
      inactivity_info_;
  std::vector<std::shared_ptr<model::Player>> players_to_retire_;
  std::chrono::milliseconds retirement_time_ = std::chrono::minutes(1);

  static const std::string EMPTY_JSON;

  void LoadConfig() {
    std::ifstream file(config_path_);
    if (!file.is_open()) {
      return;
    }
    std::string str((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());
    try {
      config_json_ = boost::json::parse(str);

      // Читаем dogRetirementTime из конфига
      if (config_json_.is_object()) {
        const auto& obj = config_json_.as_object();
        if (obj.contains("dogRetirementTime")) {
          double retirement_seconds = obj.at("dogRetirementTime").as_double();
          retirement_time_ = std::chrono::milliseconds(
              static_cast<int64_t>(retirement_seconds * 1000));
        }
      }
    } catch (...) {
      config_json_ = {};
    }
  }

  boost::json::array GetLootTypesForMap(const std::string& map_id) const {
    if (!config_json_.is_object()) {
      return {};
    }

    const auto& obj = config_json_.as_object();
    if (!obj.contains("maps")) {
      return {};
    }

    const auto& maps = obj.at("maps").as_array();
    for (const auto& map_val : maps) {
      const auto& map_obj = map_val.as_object();
      if (map_obj.at("id").as_string() == map_id) {
        if (map_obj.contains("lootTypes")) {
          return map_obj.at("lootTypes").as_array();
        }
        return {};
      }
    }
    return {};
  }

  std::optional<model::Token> ParseAuthorizationHeader(
      const http::request<http::string_body>& req) {
    auto it = req.find(http::field::authorization);
    if (it == req.end()) {
      return std::nullopt;
    }

    std::string auth_value = std::string(it->value());
    const std::string prefix = "Bearer ";
    if (auth_value.size() < prefix.size() ||
        auth_value.substr(0, prefix.size()) != prefix) {
      return std::nullopt;
    }

    std::string token_str = auth_value.substr(prefix.size());
    if (token_str.size() != 32 ||
        !std::all_of(token_str.begin(), token_str.end(),
                     [](char c) { return std::isxdigit(c); })) {
      return std::nullopt;
    }

    return model::Token(token_str);
  }

  void CheckInactiveDogs(std::chrono::milliseconds current_time) {
    // Проверяем всех игроков на всех картах
    for (auto* player : players_.GetAllPlayers()) {
      if (!player) continue;

      auto* dog = player->GetDog(&game_);
      if (!dog) continue;

      auto& info = inactivity_info_[dog->GetId()];
      bool is_moving = (dog->GetSpeedX() != 0.0 || dog->GetSpeedY() != 0.0);

      if (!is_moving && !info.is_idle) {
        // Начало бездействия
        info.is_idle = true;
        info.idle_start_time = current_time;
      } else if (is_moving && info.is_idle) {
        // Прервано бездействие
        info.is_idle = false;
      }

      // Проверяем, не превышено ли время бездействия
      if (info.is_idle) {
        auto idle_duration = current_time - info.idle_start_time;
        if (idle_duration >= retirement_time_) {
          // Добавляем игрока в список на пенсию
          auto player_ptr = players_.FindPlayerById(player->GetId());
          if (player_ptr) {
            players_to_retire_.push_back(player_ptr);
          }
        }
      }
    }
  }

  void RetirePlayer(std::shared_ptr<model::Player> player_ptr) {
    if (!player_ptr) return;

    auto* dog = player_ptr->GetDog(&game_);
    if (!dog) return;

    // Вычисляем время игры в секундах
    double play_time_seconds = game_.GetGameTime().count() / 1000.0;

    // Сохраняем рекорд
    try {
      record_manager_.AddRecord(dog->GetName(), dog->GetScore(),
                                play_time_seconds);
      std::cout << "Player retired: " << dog->GetName()
                << " score=" << dog->GetScore() << " time=" << play_time_seconds
                << "s" << std::endl;
    } catch (const std::exception& e) {
      std::cerr << "Failed to save record for retired player: " << e.what()
                << std::endl;
    }

    // Удаляем игрока
    players_.RemovePlayer(player_ptr->GetId());

    // Удаляем информацию о бездействии
    inactivity_info_.erase(dog->GetId());
  }

  template <typename Body, typename Allocator, typename Send>
  void HandleApiRequest(
      http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    std::string target(req.target());

    // Обработка /api/v1/game/records
    if (target.rfind(std::string(endpoints::GAME_RECORDS), 0) == 0) {
      if constexpr (std::is_same_v<Body, http::string_body>) {
        api_handlers::HandleRecordsRequest(req, record_manager_,
                                           std::forward<Send>(send));
      } else {
        response_helpers::SendErrorResponse(
            std::forward<Send>(send), http::status::bad_request,
            "invalidArgument", "Invalid body type");
      }
      return;
    }

    if (target == endpoints::GAME_TICK) {
      if (tick_enabled_) {
        response_helpers::SendErrorResponse(std::forward<Send>(send),
                                            http::status::bad_request,
                                            "badRequest", "Invalid endpoint");
        return;
      }
      if constexpr (std::is_same_v<Body, http::string_body>) {
        api_handlers::HandleTickRequest(std::move(req), game_,
                                        std::forward<Send>(send));
        // Сохраняем состояние ПОСЛЕ тика
        SaveState();
      } else {
        response_helpers::SendErrorResponse(
            std::forward<Send>(send), http::status::bad_request,
            "invalidArgument", "Invalid body type");
      }
      return;
    }

    if (target == endpoints::MAPS) {
      api_handlers::HandleMapsRequest(std::move(req), game_,
                                      std::forward<Send>(send));
      return;
    }

    std::regex map_detail_regex(R"(^/api/v1/maps/([^/]+)$)");
    std::smatch match;
    if (std::regex_match(target, match, map_detail_regex)) {
      api_handlers::HandleMapDetailRequest(std::move(req), game_,
                                           std::forward<Send>(send), match[1],
                                           GetLootTypesForMap(match[1]));
      return;
    }

    if (target == endpoints::GAME_JOIN) {
      if constexpr (std::is_same_v<Body, http::string_body>) {
        api_handlers::HandleJoinRequest(req, game_, players_,
                                        std::forward<Send>(send));
      } else {
        response_helpers::SendErrorResponse(
            std::forward<Send>(send), http::status::bad_request,
            "invalidArgument", "Invalid body type");
      }
      return;
    }

    if (target == endpoints::GAME_PLAYERS) {
      if constexpr (std::is_same_v<Body, http::string_body>) {
        api_handlers::HandlePlayersRequest(req, players_,
                                           std::forward<Send>(send));
      } else {
        response_helpers::SendErrorResponse(
            std::forward<Send>(send), http::status::bad_request,
            "invalidArgument", "Invalid body type");
      }
      return;
    }

    if (target == endpoints::GAME_STATE) {
      if constexpr (std::is_same_v<Body, http::string_body>) {
        api_handlers::HandleGameStateRequest(req, game_, players_,
                                             std::forward<Send>(send));
      } else {
        response_helpers::SendErrorResponse(
            std::forward<Send>(send), http::status::bad_request,
            "invalidArgument", "Invalid body type");
      }
      return;
    }

    if (target == endpoints::PLAYER_ACTION) {
      if constexpr (std::is_same_v<Body, http::string_body>) {
        api_handlers::HandlePlayerAction(std::move(req), game_, players_,
                                         std::forward<Send>(send));
      } else {
        response_helpers::SendErrorResponse(
            std::forward<Send>(send), http::status::bad_request,
            "invalidArgument", "Invalid body type");
      }
      return;
    }

    response_helpers::SendErrorResponse(std::forward<Send>(send),
                                        http::status::bad_request, "badRequest",
                                        "Bad request");
  }

  template <typename Body, typename Allocator, typename Send>
  void HandleStaticFile(
      http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
      response_helpers::SendTextErrorResponse(std::forward<Send>(send),
                                              http::status::method_not_allowed,
                                              "Method Not Allowed");
      return;
    }

    std::string decoded_target =
        url_utils::UrlDecode(std::string(req.target()));

    if (decoded_target.empty() || decoded_target == "/") {
      decoded_target = "/index.html";
    }

    std::filesystem::path relative_path = decoded_target.substr(1);
    std::filesystem::path full_path = static_dir_ / relative_path;

    if (!IsPathInsideDirectory(full_path, static_dir_)) {
      response_helpers::SendTextErrorResponse(
          std::forward<Send>(send), http::status::bad_request,
          "Bad Request: Path outside root directory");
      return;
    }

    if (std::filesystem::is_directory(full_path)) {
      full_path /= "index.html";
    }

    if (!std::filesystem::exists(full_path) ||
        !std::filesystem::is_regular_file(full_path)) {
      response_helpers::SendTextErrorResponse(
          std::forward<Send>(send), http::status::not_found, "File Not Found");
      return;
    }

    SendFileResponse(std::forward<Send>(send), full_path);
  }

  static bool IsPathInsideDirectory(const std::filesystem::path& path,
                                    const std::filesystem::path& directory) {
    try {
      auto canonical_path = std::filesystem::weakly_canonical(path);
      auto canonical_dir = std::filesystem::weakly_canonical(directory);

      auto path_it = canonical_path.begin();
      auto dir_it = canonical_dir.begin();

      while (dir_it != canonical_dir.end()) {
        if (path_it == canonical_path.end() || *path_it != *dir_it) {
          return false;
        }
        ++path_it;
        ++dir_it;
      }
      return true;
    } catch (const std::filesystem::filesystem_error&) {
      return false;
    }
  }

  template <typename Send>
  static void SendFileResponse(Send&& send,
                               const std::filesystem::path& file_path) {
    http::response<http::file_body> response;
    response.version(11);
    response.result(http::status::ok);

    std::string mime_type = mime_types::GetMimeType(file_path);
    response.set(http::field::content_type, mime_type);

    http::file_body::value_type file;
    boost::system::error_code ec;
    file.open(file_path.string().c_str(), beast::file_mode::read, ec);

    if (ec) {
      http::response<http::string_body> error_response{http::status::not_found,
                                                       11};
      error_response.set(http::field::content_type, "text/plain");
      error_response.body() = "File Not Found";
      error_response.prepare_payload();
      send(std::move(error_response));
      return;
    }

    response.body() = std::move(file);
    send(std::move(response));
  }
};

const std::string RequestHandler::EMPTY_JSON = "{}";

}  // namespace http_handler