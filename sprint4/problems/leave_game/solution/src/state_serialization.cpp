#include "state_serialization.h"

#include <boost/json.hpp>
#include <iostream>
#include <sstream>

namespace state_serialization {

boost::json::value SerializeGameState(const GameState& state) {
  boost::json::object result;

  // Сериализация потерянных предметов
  boost::json::array lost_objs;
  for (const auto& [id, obj] : state.lost_objects) {
    boost::json::object obj_json;
    obj_json["id"] = obj.id;
    obj_json["type"] = obj.type;
    obj_json["x"] = obj.x;
    obj_json["y"] = obj.y;
    lost_objs.push_back(obj_json);
  }
  result["lost_objects"] = lost_objs;

  // Сериализация игроков
  boost::json::array players_arr;
  for (const auto& player : state.players) {
    boost::json::object player_json;
    player_json["token"] = player.token;
    player_json["player_id"] = player.player_id;
    player_json["name"] = player.name;
    player_json["map_id"] = player.map_id;
    player_json["dog_id"] = player.dog_id;
    player_json["join_time_ms"] = player.join_time_ms.count();
    players_arr.push_back(player_json);
  }
  result["players"] = players_arr;

  // Сериализация собак по картам
  boost::json::object maps_dogs;
  for (const auto& [map_id, dogs] : state.map_dogs) {
    boost::json::array dogs_arr;
    for (const auto& dog : dogs) {
      boost::json::object dog_json;
      dog_json["id"] = dog.id;
      dog_json["name"] = dog.name;
      dog_json["x"] = dog.x;
      dog_json["y"] = dog.y;
      dog_json["speed_x"] = dog.speed_x;
      dog_json["speed_y"] = dog.speed_y;
      dog_json["direction"] = dog.direction;

      boost::json::array bag_arr;
      for (const auto& item : dog.bag) {
        boost::json::object item_json;
        item_json["id"] = item.id;
        item_json["type"] = item.type;
        item_json["x"] = item.position.x;
        item_json["y"] = item.position.y;
        bag_arr.push_back(item_json);
      }
      dog_json["bag"] = bag_arr;
      dog_json["score"] = dog.score;
      dogs_arr.push_back(dog_json);
    }
    maps_dogs[map_id] = dogs_arr;
  }
  result["map_dogs"] = maps_dogs;
  result["next_loot_id"] = state.next_loot_id;

  // Сохраняем игровое время
  result["game_time_ms"] = state.game_time_ms.count();

  return result;
}

GameState DeserializeGameState(const boost::json::value& json) {
  GameState state;
  const auto& obj = json.as_object();

  // Десериализация потерянных предметов
  if (obj.contains("lost_objects")) {
    const auto& lost_arr = obj.at("lost_objects").as_array();
    for (const auto& item : lost_arr) {
      const auto& item_obj = item.as_object();
      LostObjectState lost_obj;
      lost_obj.id = item_obj.at("id").as_int64();
      lost_obj.type = item_obj.at("type").as_int64();
      lost_obj.x = item_obj.at("x").as_double();
      lost_obj.y = item_obj.at("y").as_double();
      state.lost_objects[lost_obj.id] = lost_obj;
    }
  }

  // Десериализация игроков
  if (obj.contains("players")) {
    const auto& players_arr = obj.at("players").as_array();
    for (const auto& item : players_arr) {
      const auto& player_obj = item.as_object();
      PlayerState player;
      player.token = player_obj.at("token").as_string().c_str();
      player.player_id = player_obj.at("player_id").as_int64();

      if (player_obj.contains("name")) {
        player.name = player_obj.at("name").as_string().c_str();
      } else {
        player.name = "Restored";
      }

      player.map_id = player_obj.at("map_id").as_string().c_str();
      player.dog_id = player_obj.at("dog_id").as_int64();
      if (player_obj.contains("join_time_ms")) {
        player.join_time_ms = std::chrono::milliseconds(
            player_obj.at("join_time_ms").as_int64());
      } else {
        player.join_time_ms = state.game_time_ms;
      }
      state.players.push_back(player);
    }
  }

  // Десериализация собак по картам
  if (obj.contains("map_dogs")) {
    const auto& maps_dogs_obj = obj.at("map_dogs").as_object();
    for (const auto& [map_id_view, dogs_val] : maps_dogs_obj) {
      std::string map_id = std::string(map_id_view.data(), map_id_view.size());

      const auto& dogs_arr = dogs_val.as_array();
      std::vector<DogState> dogs;
      for (const auto& dog_val : dogs_arr) {
        const auto& dog_obj = dog_val.as_object();
        DogState dog;
        dog.id = dog_obj.at("id").as_int64();
        dog.name = dog_obj.at("name").as_string().c_str();
        dog.x = dog_obj.at("x").as_double();
        dog.y = dog_obj.at("y").as_double();
        dog.speed_x = dog_obj.at("speed_x").as_double();
        dog.speed_y = dog_obj.at("speed_y").as_double();
        dog.direction = dog_obj.at("direction").as_string().c_str();

        if (dog_obj.contains("bag")) {
          const auto& bag_arr = dog_obj.at("bag").as_array();
          for (const auto& item_val : bag_arr) {
            const auto& item_obj = item_val.as_object();
            model::LostObject item;
            item.id = item_obj.at("id").as_int64();
            item.type = item_obj.at("type").as_int64();
            item.position.x = item_obj.at("x").as_double();
            item.position.y = item_obj.at("y").as_double();
            dog.bag.push_back(item);
          }
        }
        dog.score = dog_obj.at("score").as_int64();
        dogs.push_back(dog);
      }
      state.map_dogs[map_id] = dogs;
    }
  }

  if (obj.contains("next_loot_id")) {
    state.next_loot_id = obj.at("next_loot_id").as_int64();
  }

  if (obj.contains("game_time_ms")) {
    state.game_time_ms =
        std::chrono::milliseconds(obj.at("game_time_ms").as_int64());
  }

  return state;
}

GameState ToGameState(const model::Game& game, const model::Players& players) {
  GameState state;

  // Сохраняем потерянные предметы
  for (const auto& [id, obj] : game.GetLostObjects()) {
    LostObjectState lost_obj;
    lost_obj.id = obj.id;
    lost_obj.type = obj.type;
    lost_obj.x = obj.position.x;
    lost_obj.y = obj.position.y;
    state.lost_objects[id] = lost_obj;
  }

  // Сохраняем собак по картам
  for (const auto& map : game.GetMaps()) {
    std::vector<DogState> dogs;
    for (const auto& dog : map.GetDogs()) {
      DogState dog_state;
      dog_state.id = *dog.GetId();
      dog_state.name = dog.GetName();
      auto pos = dog.GetPosition();
      dog_state.x = pos.x;
      dog_state.y = pos.y;
      dog_state.speed_x = dog.GetSpeedX();
      dog_state.speed_y = dog.GetSpeedY();
      dog_state.direction = dog.DirectionToString();
      dog_state.bag = dog.GetBag();
      dog_state.score = dog.GetScore();
      dogs.push_back(dog_state);
    }
    state.map_dogs[*map.GetId()] = dogs;
  }

  // Сохраняем игроков с токенами и именами
  for (const auto* player : players.GetAllPlayers()) {
    PlayerState player_state;
    player_state.player_id = *player->GetId();
    player_state.name = player->GetName();
    player_state.map_id = *player->GetMapId();
    player_state.dog_id = *player->GetDogId();
    player_state.join_time_ms = player->GetJoinTime();

    auto token_opt = players.GetTokenByPlayerId(player->GetId());
    if (token_opt) {
      player_state.token = **token_opt;
    } else {
      std::cerr << "Warning: Token not found for player " << *player->GetId()
                << std::endl;
    }
    state.players.push_back(player_state);
  }

  state.next_loot_id = game.GetNextLootId();
  state.game_time_ms = game.GetGameTime();

  std::cout << "ToGameState: saved " << state.players.size() << " players"
            << std::endl;

  return state;
}

void RestoreGameState(model::Game& game, model::Players& players,
                      const GameState& state) {
  // Восстанавливаем потерянные предметы
  auto& lost_objects = game.GetLostObjectsMutable();
  lost_objects.clear();
  for (const auto& [id, obj] : state.lost_objects) {
    model::LostObject restored_obj;
    restored_obj.id = obj.id;
    restored_obj.type = obj.type;
    restored_obj.position.x = obj.x;
    restored_obj.position.y = obj.y;
    lost_objects[id] = restored_obj;
  }

  game.SetNextLootId(state.next_loot_id);

  // Восстанавливаем игровое время
  game.SetGameTime(state.game_time_ms);

  // ====== ВАЖНО: Сначала очищаем все карты и игроков ======
  game.ClearDogs();
  players.ClearPlayers();

  // ====== Восстанавливаем собак на картах ======
  for (auto& map : game.GetMaps()) {
    auto& dogs = map.GetDogsMutable();
    dogs.clear();

    auto it = state.map_dogs.find(*map.GetId());
    if (it != state.map_dogs.end()) {
      for (const auto& dog_state : it->second) {
        model::Dog::Id dog_id(dog_state.id);
        model::Dog dog(dog_id, dog_state.name, {dog_state.x, dog_state.y});
        dog.SetSpeed(dog_state.speed_x, dog_state.speed_y);

        if (dog_state.direction == "U") {
          dog.SetDirection(model::Dog::Direction::NORTH);
        } else if (dog_state.direction == "D") {
          dog.SetDirection(model::Dog::Direction::SOUTH);
        } else if (dog_state.direction == "L") {
          dog.SetDirection(model::Dog::Direction::WEST);
        } else if (dog_state.direction == "R") {
          dog.SetDirection(model::Dog::Direction::EAST);
        }

        dog.SetScore(dog_state.score);
        auto& bag = dog.GetBagMutable();
        bag.clear();
        for (const auto& item : dog_state.bag) {
          bag.push_back(item);
        }
        dogs.push_back(std::move(dog));
      }
    }
  }

  // ====== Восстанавливаем игроков с токенами ======
  for (const auto& player_state : state.players) {
    try {
      players.RestorePlayerWithToken(
          player_state.token, player_state.player_id, player_state.name,
          model::Map::Id(player_state.map_id), player_state.dog_id,
          player_state.join_time_ms);
    } catch (const std::exception& e) {
      std::cerr << "Error restoring player " << player_state.player_id << ": "
                << e.what() << std::endl;
    }
  }

  std::cout << "RestoreGameState: restored " << state.players.size()
            << " players" << std::endl;
}

}  // namespace state_serialization