#pragma once
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <regex>
#include <sstream>  // <-- ДОБАВИТЬ для std::istringstream

#include "game_state.h"
#include "http_server.h"
#include "json_loader.h"
#include "model.h"
#include "players.h"
#include "record_manager.h"  // <-- ДОБАВИТЬ
#include "response_helpers.h"
#include "serializers.h"

namespace http_handler {
namespace api_handlers {

namespace beast = boost::beast;
namespace http = beast::http;

template <typename Send>
void HandleJoinRequest(const http::request<http::string_body>& req,
                       model::Game& game, model::Players& players,
                       Send&& send) {
  if (req.method() != http::verb::post) {
    response_helpers::SendErrorResponseWithAllow(
        std::forward<Send>(send), http::status::method_not_allowed,
        "invalidMethod", "Only POST method is expected", "POST");
    return;
  }

  auto content_type_it = req.find(http::field::content_type);
  if (content_type_it == req.end() ||
      std::string(content_type_it->value()) != "application/json") {
    response_helpers::SendErrorResponse(
        std::forward<Send>(send), http::status::bad_request, "invalidArgument",
        "Invalid Content-Type");
    return;
  }

  try {
    boost::json::value json_value = boost::json::parse(req.body());
    if (!json_value.is_object()) {
      throw std::runtime_error("Invalid JSON format");
    }

    auto& json_obj = json_value.as_object();

    if (!json_obj.contains("userName") || !json_obj.contains("mapId")) {
      throw std::runtime_error("Missing required fields");
    }

    if (!json_obj["userName"].is_string() || !json_obj["mapId"].is_string()) {
      throw std::runtime_error("Invalid field types");
    }

    std::string user_name = json_obj["userName"].as_string().c_str();
    std::string map_id_str = json_obj["mapId"].as_string().c_str();

    if (user_name.empty()) {
      response_helpers::SendErrorResponse(std::forward<Send>(send),
                                          http::status::bad_request,
                                          "invalidArgument", "Invalid name");
      return;
    }

    model::Map::Id map_id(map_id_str);
    const model::Map* map = game.FindMap(map_id);
    if (!map) {
      response_helpers::SendErrorResponse(std::forward<Send>(send),
                                          http::status::not_found,
                                          "mapNotFound", "Map not found");
      return;
    }

    auto [player_id, token] = players.AddPlayer(user_name, map_id);

    boost::json::object response_obj;
    response_obj["authToken"] = *token;
    response_obj["playerId"] = *player_id;

    http::response<http::string_body> response{http::status::ok, 11};
    response.set(http::field::content_type, "application/json");
    response.set(http::field::cache_control, "no-cache");
    response.body() = boost::json::serialize(response_obj);
    response.prepare_payload();
    send(std::move(response));

  } catch (const boost::system::system_error& e) {
    response_helpers::SendErrorResponse(
        std::forward<Send>(send), http::status::bad_request, "invalidArgument",
        "Join game request parse error");
  } catch (const std::exception& e) {
    response_helpers::SendErrorResponse(
        std::forward<Send>(send), http::status::bad_request, "invalidArgument",
        "Join game request parse error");
  }
}

template <typename Send>
void HandlePlayersRequest(const http::request<http::string_body>& req,
                          model::Players& players, Send&& send) {
  if (req.method() != http::verb::get && req.method() != http::verb::head) {
    response_helpers::SendErrorResponseWithAllow(
        std::forward<Send>(send), http::status::method_not_allowed,
        "invalidMethod", "Invalid method", "GET, HEAD");
    return;
  }

  auto auth_it = req.find(http::field::authorization);
  if (auth_it == req.end()) {
    response_helpers::SendErrorResponse(
        std::forward<Send>(send), http::status::unauthorized, "invalidToken",
        "Authorization header missing");
    return;
  }

  std::string auth_value = std::string(auth_it->value());
  const std::string prefix = "Bearer ";
  if (auth_value.size() < prefix.size() ||
      auth_value.substr(0, prefix.size()) != prefix) {
    response_helpers::SendErrorResponse(
        std::forward<Send>(send), http::status::unauthorized, "invalidToken",
        "Invalid authorization header");
    return;
  }

  std::string token_str = auth_value.substr(prefix.size());
  model::Token token(token_str);

  auto player_ptr = players.FindPlayerByToken(token);
  if (!player_ptr) {
    response_helpers::SendErrorResponse(
        std::forward<Send>(send), http::status::unauthorized, "unknownToken",
        "Player token not found");
    return;
  }

  auto players_on_map = players.GetPlayersByMap(player_ptr->GetMapId());

  boost::json::object response_obj;
  for (const auto* p : players_on_map) {
    boost::json::object player_info;
    player_info["name"] = p->GetName();
    response_obj[std::to_string(*p->GetId())] = player_info;
  }

  http::response<http::string_body> response{http::status::ok, 11};
  response.set(http::field::content_type, "application/json");
  response.set(http::field::cache_control, "no-cache");
  response.body() = boost::json::serialize(response_obj);
  response.prepare_payload();
  send(std::move(response));
}

template <typename Send>
void HandleGameStateRequest(const http::request<http::string_body>& req,
                            model::Game& game, model::Players& players,
                            Send&& send) {
  if (req.method() != http::verb::get && req.method() != http::verb::head) {
    response_helpers::SendErrorResponseWithAllow(
        std::forward<Send>(send), http::status::method_not_allowed,
        "invalidMethod", "Invalid method", "GET, HEAD");
    return;
  }

  auto auth_it = req.find(http::field::authorization);
  if (auth_it == req.end()) {
    response_helpers::SendErrorResponse(
        std::forward<Send>(send), http::status::unauthorized, "invalidToken",
        "Authorization header missing");
    return;
  }

  std::string auth_value = std::string(auth_it->value());
  const std::string prefix = "Bearer ";
  if (auth_value.size() < prefix.size() ||
      auth_value.substr(0, prefix.size()) != prefix) {
    response_helpers::SendErrorResponse(
        std::forward<Send>(send), http::status::unauthorized, "invalidToken",
        "Invalid authorization header");
    return;
  }

  std::string token_str = auth_value.substr(prefix.size());
  model::Token token(token_str);

  auto player_ptr = players.FindPlayerByToken(token);
  if (!player_ptr) {
    response_helpers::SendErrorResponse(
        std::forward<Send>(send), http::status::unauthorized, "unknownToken",
        "Player token not found");
    return;
  }

  auto players_on_map = players.GetPlayersByMap(player_ptr->GetMapId());

  boost::json::object response_obj;
  boost::json::object players_obj;

  for (const auto* p : players_on_map) {
    boost::json::object player_info;

    const auto* dog = const_cast<model::Player*>(p)->GetDog(&game);
    if (dog) {
      const auto& pos = dog->GetPosition();
      player_info["pos"] = boost::json::array({pos.x, pos.y});

      player_info["speed"] =
          boost::json::array({dog->GetSpeedX(), dog->GetSpeedY()});

      player_info["dir"] = dog->DirectionToString();

      boost::json::array bag_array;
      for (const auto& item : dog->GetBag()) {
        boost::json::object item_obj;
        item_obj["id"] = item.id;
        item_obj["type"] = item.type;
        bag_array.push_back(item_obj);
      }
      player_info["bag"] = bag_array;

      player_info["score"] = dog->GetScore();

    } else {
      player_info["pos"] = boost::json::array({0.0, 0.0});
      player_info["speed"] = boost::json::array({0.0, 0.0});
      player_info["dir"] = "U";
      player_info["bag"] = boost::json::array();
      player_info["score"] = 0;
    }

    players_obj[std::to_string(*p->GetId())] = player_info;
  }

  response_obj["players"] = players_obj;

  boost::json::object lost_objects;
  for (const auto& [id, obj] : game.GetLostObjects()) {
    boost::json::object obj_json;
    obj_json["type"] = obj.type;
    obj_json["pos"] = boost::json::array({obj.position.x, obj.position.y});
    lost_objects[std::to_string(id)] = obj_json;
  }
  response_obj["lostObjects"] = lost_objects;

  http::response<http::string_body> response{http::status::ok, 11};
  response.set(http::field::content_type, "application/json");
  response.set(http::field::cache_control, "no-cache");
  response.body() = boost::json::serialize(response_obj);
  response.prepare_payload();
  send(std::move(response));
}

      // Форматируем playTime: если целое число, без десятичных
template <typename Body, typename Allocator, typename Send>
void HandlePlayerAction(
    http::request<Body, http::basic_fields<Allocator>>&& req, model::Game& game,
    model::Players& players, Send&& send) {
  if (req.method() != http::verb::post) {
    response_helpers::SendErrorResponseWithAllow(
        std::forward<Send>(send), http::status::method_not_allowed,
        "invalidMethod", "Invalid method", "POST");
    return;
  }

  auto content_type_it = req.find(http::field::content_type);
  if (content_type_it == req.end() ||
      std::string(content_type_it->value()) != "application/json") {
    response_helpers::SendErrorResponse(
        std::forward<Send>(send), http::status::bad_request, "invalidArgument",
        "Invalid content type");
    return;
  }

  auto auth_it = req.find(http::field::authorization);
  if (auth_it == req.end()) {
    response_helpers::SendErrorResponse(
        std::forward<Send>(send), http::status::unauthorized, "invalidToken",
        "Authorization header missing");
    return;
  }

  std::string auth_value = std::string(auth_it->value());
  const std::string prefix = "Bearer ";
  if (auth_value.size() < prefix.size() ||
      auth_value.substr(0, prefix.size()) != prefix) {
    response_helpers::SendErrorResponse(
        std::forward<Send>(send), http::status::unauthorized, "invalidToken",
        "Invalid authorization header");
    return;
  }

  std::string token_str = auth_value.substr(prefix.size());
  model::Token token(token_str);

  auto player_ptr = players.FindPlayerByToken(token);
  if (!player_ptr) {
    response_helpers::SendErrorResponse(
        std::forward<Send>(send), http::status::unauthorized, "unknownToken",
        "Player token not found");
    return;
  }

  // ===== ДОБАВЛЕННАЯ ПРОВЕРКА =====
  const model::Player* player = player_ptr.get();
  if (!player) {
    response_helpers::SendErrorResponse(std::forward<Send>(send),
                                        http::status::unauthorized,
                                        "unknownToken", "Player not found");
    return;
  }
  // ===== КОНЕЦ ПРОВЕРКИ =====

  try {
    boost::json::value json_value = boost::json::parse(req.body());
    if (!json_value.is_object()) {
      throw std::runtime_error("Invalid JSON format");
    }

    auto& json_obj = json_value.as_object();
    if (!json_obj.contains("move") || !json_obj["move"].is_string()) {
      throw std::runtime_error("Missing or invalid 'move' field");
    }

    std::string move_direction = json_obj["move"].as_string().c_str();

    model::Dog* dog = const_cast<model::Player*>(player)->GetDog(&game);
    if (!dog) {
      throw std::runtime_error("Dog not found");
    }

    const model::Map* map = game.FindMap(player->GetMapId());
    if (!map) {
      throw std::runtime_error("Map not found");
    }

    double speed = map->GetDogSpeed();

    if (move_direction == "L") {
      dog->SetSpeed(-speed, 0);
      dog->SetDirection(model::Dog::Direction::WEST);
    } else if (move_direction == "R") {
      dog->SetSpeed(speed, 0);
      dog->SetDirection(model::Dog::Direction::EAST);
    } else if (move_direction == "U") {
      dog->SetSpeed(0, -speed);
      dog->SetDirection(model::Dog::Direction::NORTH);
    } else if (move_direction == "D") {
      dog->SetSpeed(0, speed);
      dog->SetDirection(model::Dog::Direction::SOUTH);
    } else if (move_direction == "") {
      dog->SetSpeed(0, 0);
    } else {
      throw std::runtime_error("Invalid move direction");
    }

    http::response<http::string_body> response{http::status::ok, 11};
    response.set(http::field::content_type, "application/json");
    response.set(http::field::cache_control, "no-cache");
    response.body() = "{}";
    response.prepare_payload();
    send(std::move(response));

  } catch (const boost::system::system_error& e) {
    response_helpers::SendErrorResponse(
        std::forward<Send>(send), http::status::bad_request, "invalidArgument",
        "Failed to parse action");
  } catch (const std::exception& e) {
    response_helpers::SendErrorResponse(
        std::forward<Send>(send), http::status::bad_request, "invalidArgument",
        "Failed to parse action");
  }
}

template <typename Body, typename Allocator, typename Send>
void HandleTickRequest(http::request<Body, http::basic_fields<Allocator>>&& req,
                       model::Game& game, Send&& send) {
  if (req.method() != http::verb::post) {
    response_helpers::SendErrorResponseWithAllow(
        std::forward<Send>(send), http::status::method_not_allowed,
        "invalidMethod", "Only POST method is expected", "POST");
    return;
  }

  auto content_type_it = req.find(http::field::content_type);
  if (content_type_it == req.end() ||
      std::string(content_type_it->value()) != "application/json") {
    response_helpers::SendErrorResponse(
        std::forward<Send>(send), http::status::bad_request, "invalidArgument",
        "Invalid Content-Type");
    return;
  }

  try {
    boost::json::value json_value = boost::json::parse(req.body());
    if (!json_value.is_object()) {
      throw std::runtime_error("Invalid JSON format");
    }

    auto& json_obj = json_value.as_object();

    if (!json_obj.contains("timeDelta")) {
      throw std::runtime_error("Missing 'timeDelta' field");
    }

    if (!json_obj["timeDelta"].is_int64()) {
      throw std::runtime_error("Invalid 'timeDelta' type");
    }

    int64_t time_delta_ms = json_obj["timeDelta"].as_int64();

    if (time_delta_ms < 0) {
      throw std::runtime_error("timeDelta must be non-negative");
    }

    auto delta_time = std::chrono::milliseconds(time_delta_ms);
    auto current_time = game.GetGameTime() + delta_time;

    // Временные переменные для вызова функции с 7 аргументами
    std::unordered_map<model::Dog::Id, game_state::DogInactivityInfo,
                       util::TaggedHasher<model::Dog::Id>>
        inactivity_info;
    std::vector<std::shared_ptr<model::Player>> players_to_retire;
    std::chrono::milliseconds retirement_time = std::chrono::minutes(1);

    for (auto& map : game.GetMaps()) {
      game_state::UpdateDogsPositionAndGather(
          map, delta_time, game.GetLostObjectsMutable(), current_time,
          retirement_time, inactivity_info, players_to_retire);
    }

    game.UpdateTime(delta_time);

    http::response<http::string_body> response{http::status::ok, 11};
    response.set(http::field::content_type, "application/json");
    response.set(http::field::cache_control, "no-cache");
    response.body() = "{}";
    response.prepare_payload();
    send(std::move(response));

  } catch (const boost::system::system_error& e) {
    response_helpers::SendErrorResponse(
        std::forward<Send>(send), http::status::bad_request, "invalidArgument",
        "Failed to parse tick request JSON");
  } catch (const std::exception& e) {
    response_helpers::SendErrorResponse(std::forward<Send>(send),
                                        http::status::bad_request,
                                        "invalidArgument", e.what());
  }
}

template <typename Send>
void HandleMapsRequest(http::request<http::string_body>&& req,
                       model::Game& game, Send&& send) {
  if (req.method() != http::verb::get && req.method() != http::verb::head) {
    response_helpers::SendErrorResponseWithAllow(
        std::forward<Send>(send), http::status::method_not_allowed,
        "invalidMethod", "Only GET/HEAD method is expected", "GET, HEAD");
    return;
  }

  boost::json::array maps_array;
  for (const auto& map : game.GetMaps()) {
    boost::json::object map_obj;
    map_obj["id"] = *map.GetId();
    map_obj["name"] = map.GetName();
    maps_array.push_back(map_obj);
  }

  http::response<http::string_body> response{http::status::ok, 11};
  response.set(http::field::content_type, "application/json");
  response.set(http::field::cache_control, "no-cache");
  response.body() = boost::json::serialize(maps_array);
  response.prepare_payload();
  send(std::move(response));
}

template <typename Send>
void HandleMapDetailRequest(http::request<http::string_body>&& req,
                            model::Game& game, Send&& send,
                            const std::string& map_id,
                            const boost::json::array& loot_types) {
  if (req.method() != http::verb::get && req.method() != http::verb::head) {
    response_helpers::SendErrorResponseWithAllow(
        std::forward<Send>(send), http::status::method_not_allowed,
        "invalidMethod", "Only GET/HEAD method is expected", "GET, HEAD");
    return;
  }

  const model::Map* map = game.FindMap(model::Map::Id(map_id));
  if (!map) {
    response_helpers::SendErrorResponse(std::forward<Send>(send),
                                        http::status::not_found, "mapNotFound",
                                        "Map not found");
    return;
  }

  boost::json::object map_obj = serializers::SerializeMapInfo(*map);
  map_obj["lootTypes"] = loot_types;

  http::response<http::string_body> response{http::status::ok, 11};
  response.set(http::field::content_type, "application/json");
  response.set(http::field::cache_control, "no-cache");
  response.body() = boost::json::serialize(map_obj);
  response.prepare_payload();
  send(std::move(response));
}

template <typename Send>
void HandleRecordsRequest(const http::request<http::string_body>& req,
                          const records::RecordManager& record_manager,
                          Send&& send) {
  if (req.method() != http::verb::get && req.method() != http::verb::head) {
    response_helpers::SendErrorResponseWithAllow(
        std::forward<Send>(send), http::status::method_not_allowed,
        "invalidMethod", "Only GET/HEAD method is expected", "GET, HEAD");
    return;
  }

  // Разбор параметров start и maxItems
  int start = 0;
  int maxItems = 100;

  std::string target(req.target());
  auto pos = target.find('?');
  if (pos != std::string::npos) {
    std::string query = target.substr(pos + 1);
    std::istringstream iss(query);
    std::string param;
    while (std::getline(iss, param, '&')) {
      auto eq_pos = param.find('=');
      if (eq_pos == std::string::npos) continue;

      std::string key = param.substr(0, eq_pos);
      std::string value = param.substr(eq_pos + 1);

      if (key == "start") {
        try {
          start = std::stoi(value);
          if (start < 0) start = 0;
        } catch (...) {
        }
      } else if (key == "maxItems") {
        try {
          maxItems = std::stoi(value);
          if (maxItems < 1) maxItems = 1;
        } catch (...) {
        }
      }
    }
  }

  // Проверка: maxItems не должен превышать 100
  if (maxItems > 100) {
    response_helpers::SendErrorResponse(
        std::forward<Send>(send), http::status::bad_request, "invalidArgument",
        "maxItems cannot exceed 100");
    return;
  }

  try {
    auto entries = record_manager.GetRecords(start, maxItems);

    boost::json::array json_entries;
    for (const auto& entry : entries) {
      boost::json::object obj;
      obj["name"] = entry.name;
      obj["score"] = entry.score;

      if (entry.play_time_seconds == std::floor(entry.play_time_seconds)) {
        obj["playTime"] = static_cast<int64_t>(entry.play_time_seconds);
      } else {
        obj["playTime"] = entry.play_time_seconds;
      }

      json_entries.push_back(obj);
    }

    http::response<http::string_body> response{http::status::ok, 11};
    response.set(http::field::content_type, "application/json");
    response.set(http::field::cache_control, "no-cache");
    response.body() = boost::json::serialize(json_entries);
    response.prepare_payload();
    send(std::move(response));

  } catch (const std::exception& e) {
    response_helpers::SendErrorResponse(
        std::forward<Send>(send), http::status::internal_server_error,
        "internalError", "Failed to retrieve records");
  }
}

}  // namespace api_handlers
}  // namespace http_handler