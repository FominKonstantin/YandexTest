#pragma once

#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "player.h"
#include "player_tokens.h"

namespace model {

class Players {
 public:
  std::vector<Player*> GetAllPlayers() const {
    std::vector<Player*> result;
    result.reserve(players_by_id_.size());
    for (const auto& [id, player] : players_by_id_) {
      result.push_back(player.get());
    }
    return result;
  }

  std::shared_ptr<Player> FindPlayerById(PlayerId id) const {
    auto it = players_by_id_.find(id);
    if (it == players_by_id_.end()) {
      return nullptr;
    }
    return it->second;
  }

  void RemovePlayer(PlayerId id) {
    for (auto it = token_to_player_.begin(); it != token_to_player_.end();
         ++it) {
      if (it->second == id) {
        token_to_player_.erase(it);
        break;
      }
    }
    players_by_id_.erase(id);
  }

  bool HasPlayer(PlayerId id) const {
    return players_by_id_.find(id) != players_by_id_.end();
  }

  std::optional<Token> GetTokenByPlayerId(PlayerId id) const {
    for (const auto& [token, player_id] : token_to_player_) {
      if (player_id == id) {
        return token;
      }
    }
    return std::nullopt;
  }

  // Восстановить игрока из сохраненного состояния
  void RestorePlayerWithToken(const std::string& token_str, int player_id,
                              const std::string& name, const Map::Id& map_id,
                              int dog_id) {
    Token token(token_str);
    PlayerId id(player_id);

    // Проверяем, что карта существует
    Map* map = game_->FindMap(map_id);
    if (!map) {
      throw std::runtime_error("Map not found during restore");
    }

    // Проверяем, что собака с таким ID существует на карте
    Dog* dog = map->FindDog(Dog::Id(dog_id));
    if (!dog) {
      throw std::runtime_error("Dog not found during restore");
    }

    auto player = std::make_shared<Player>(id, name, map_id, Dog::Id(dog_id));
    players_by_id_.emplace(id, player);
    token_to_player_.emplace(token, id);
  }

  // Очистить всех игроков
  void ClearPlayers() {
    players_by_id_.clear();
    token_to_player_.clear();
  }

  explicit Players(model::Game* game, bool randomize_spawn = false)
      : game_(game),
        next_player_id_(0),
        next_dog_id_(0),
        randomize_spawn_(randomize_spawn) {
    std::random_device rd;
    gen_.seed(rd());
  }

  std::pair<PlayerId, Token> AddPlayer(const std::string& name,
                                       const Map::Id& map_id) {
    Map* map = game_->FindMap(map_id);
    if (!map) {
      throw std::runtime_error("Map not found");
    }

    Dog::Id dog_id(next_dog_id_++);
    Dog dog = CreateDogOnRoad(map, dog_id, name);
    dog.SetSpeed(0, 0);
    dog.SetDirection(Dog::Direction::NORTH);

    map->AddDog(std::move(dog));

    PlayerId id(next_player_id_++);
    Token token = token_generator_.GenerateToken();

    auto player = std::make_shared<Player>(id, name, map_id, dog_id);

    players_by_id_.emplace(id, player);
    token_to_player_.emplace(token, id);

    return {id, token};
  }

  std::shared_ptr<Player> FindPlayerByToken(const Token& token) const {
    auto it = token_to_player_.find(token);
    if (it == token_to_player_.end()) {
      return nullptr;
    }
    auto player_it = players_by_id_.find(it->second);
    if (player_it == players_by_id_.end()) {
      return nullptr;
    }
    return player_it->second;
  }

  std::vector<Player*> GetPlayersOnMap(const Map::Id& map_id) const {
    std::vector<Player*> result;
    for (const auto& [id, player] : players_by_id_) {
      if (player->GetMapId() == map_id) {
        result.push_back(player.get());
      }
    }
    return result;
  }

  std::vector<Player*> GetPlayersByMap(const Map::Id& map_id) const {
    return GetPlayersOnMap(map_id);
  }

  bool HasToken(const Token& token) const {
    return token_to_player_.find(token) != token_to_player_.end();
  }

 private:
  Dog CreateDogOnRoad(Map* map, Dog::Id dog_id, const std::string& name) {
    const auto& roads = map->GetRoads();
    if (roads.empty()) {
      throw std::runtime_error("No roads on map");
    }

    if (randomize_spawn_) {
      std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
      const auto& road = roads[road_dist(gen_)];

      if (road.IsHorizontal()) {
        double min_x = std::min(road.GetStart().x, road.GetEnd().x);
        double max_x = std::max(road.GetStart().x, road.GetEnd().x);
        std::uniform_real_distribution<double> x_dist(min_x, max_x);
        double x = x_dist(gen_);
        double y = road.GetStart().y;
        return Dog(dog_id, name, {x, y});
      }

      double min_y = std::min(road.GetStart().y, road.GetEnd().y);
      double max_y = std::max(road.GetStart().y, road.GetEnd().y);
      std::uniform_real_distribution<double> y_dist(min_y, max_y);
      double x = road.GetStart().x;
      double y = y_dist(gen_);
      return Dog(dog_id, name, {x, y});
    }

    const model::Road* vertical_road = nullptr;
    for (const auto& road : roads) {
      if (road.IsVertical()) {
        vertical_road = &road;
        break;
      }
    }

    const auto& road = vertical_road ? *vertical_road : roads[0];

    double x = road.GetStart().x;
    double y = road.GetStart().y;

    return Dog(dog_id, name, {x, y});
  }

  struct PlayerIdHash {
    size_t operator()(const PlayerId& id) const {
      return std::hash<int>{}(*id);
    }
  };

  struct TokenHash {
    size_t operator()(const Token& token) const {
      return std::hash<std::string>{}(*token);
    }
  };

  model::Game* game_;
  int next_player_id_;
  int next_dog_id_;
  std::mt19937 gen_;
  PlayerTokens token_generator_;
  bool randomize_spawn_;

  std::unordered_map<PlayerId, std::shared_ptr<Player>, PlayerIdHash>
      players_by_id_;
  std::unordered_map<Token, PlayerId, TokenHash> token_to_player_;
};

}  // namespace model