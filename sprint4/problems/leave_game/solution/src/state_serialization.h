#pragma once

#include <boost/json.hpp>
#include <unordered_map>
#include <vector>

#include "model.h"
#include "players.h"

namespace state_serialization {

// Структура для сериализации собаки
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

// Структура для сериализации потерянного предмета
struct LostObjectState {
  int id;
  int type;
  double x;
  double y;
};

// Структура для сериализации игрока
struct PlayerState {
  std::string token;
  int player_id;
  std::string name;  // Добавлено поле для имени
  std::string map_id;
  int dog_id;
  std::chrono::milliseconds join_time_ms = std::chrono::milliseconds::zero();
};

// Структура для сериализации всей игры
struct GameState {
  std::unordered_map<int, LostObjectState> lost_objects;
  std::vector<PlayerState> players;
  // Для каждой карты сохраняем собак
  std::unordered_map<std::string, std::vector<DogState>> map_dogs;
  int next_loot_id = 0;
  std::chrono::milliseconds game_time_ms =
      std::chrono::milliseconds::zero();  // Игровое время
};

// Сериализация GameState в JSON
boost::json::value SerializeGameState(const GameState& state);

// Десериализация GameState из JSON
GameState DeserializeGameState(const boost::json::value& json);

// Преобразование model::Game в GameState
GameState ToGameState(const model::Game& game, const model::Players& players);

// Восстановление model::Game из GameState
void RestoreGameState(model::Game& game, model::Players& players,
                      const GameState& state);

}  // namespace state_serialization