#pragma once

#include <boost/json.hpp>
#include <unordered_map>
#include <vector>

#include "model.h"
#include "players.h"

namespace state_serialization {

struct DogState {
  int id;
  std::string name;
  double x;
  double y;
  double speed_x;
  double speed_y;
  std::string direction;
  std::vector<model::LostObject> bag;
  int score;
};

struct LostObjectState {
  int id;
  int type;
  double x;
  double y;
};

struct PlayerState {
  std::string token;
  int player_id;
  std::string name; 
  std::string map_id;
  int dog_id;
  std::chrono::milliseconds join_time_ms = std::chrono::milliseconds::zero();
};

struct GameState {
  std::unordered_map<int, LostObjectState> lost_objects;
  std::vector<PlayerState> players;
  std::unordered_map<std::string, std::vector<DogState>> map_dogs;
  int next_loot_id = 0;
  std::chrono::milliseconds game_time_ms =
      std::chrono::milliseconds::zero();  
};

boost::json::value SerializeGameState(const GameState& state);

GameState DeserializeGameState(const boost::json::value& json);

GameState ToGameState(const model::Game& game, const model::Players& players);

void RestoreGameState(model::Game& game, model::Players& players,
                      const GameState& state);

}  // namespace state_serialization