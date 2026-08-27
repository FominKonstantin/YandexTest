#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "model.h"
#include "tagged.h"

namespace model {

namespace detail {
struct PlayerIdTag {};
} // namespace detail

using PlayerId = util::Tagged<int, detail::PlayerIdTag>;

class Player {
public:
  Player() = default;

  Player(PlayerId id, std::string name, const Map::Id& map_id, Dog::Id dog_id,
         std::chrono::milliseconds join_time = std::chrono::milliseconds::zero())
      : id_(id),
        name_(std::move(name)),
        map_id_(map_id),
        dog_id_(dog_id),
        join_time_(join_time) {}

  PlayerId GetId() const noexcept { return id_; }
  const std::string &GetName() const noexcept { return name_; }
  const Map::Id &GetMapId() const noexcept { return map_id_; }
  Dog::Id GetDogId() const noexcept { return dog_id_; }
  std::chrono::milliseconds GetJoinTime() const noexcept { return join_time_; }

  model::Dog *GetDog(model::Game *game) const {
    Map *map = game->FindMap(map_id_);
    if (!map) {
      return nullptr;
    }
    return map->FindDog(dog_id_);
  }

private:
  PlayerId id_;
  std::string name_;
  Map::Id map_id_;
  Dog::Id dog_id_;
  std::chrono::milliseconds join_time_ = std::chrono::milliseconds::zero();
};

} // namespace model