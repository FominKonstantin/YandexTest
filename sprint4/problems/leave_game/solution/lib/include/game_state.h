#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "model.h"
#include "tagged.h"
#include "player.h"  // <-- ДОБАВИТЬ ЭТУ СТРОКУ

namespace game_state {

constexpr double ROAD_HALF_WIDTH = 0.4;

struct DogInactivityInfo {
  std::chrono::milliseconds idle_start_time = std::chrono::milliseconds::zero();
  bool is_idle = false;
};

void UpdateDogsPosition(::model::Map& map,
                        std::chrono::milliseconds delta_time);

void MoveDogOnRoad(::model::Dog& dog, const ::model::Map& map,
                   std::chrono::milliseconds delta_time);

std::optional<::model::Road> FindRoadContainingPoint(
    const ::model::Map& map, double x, double y, bool prefer_vertical = false);

double ClampPosition(double value, double min, double max);

void ProcessGathering(
    ::model::Map& map, double dt,
    std::unordered_map<int, ::model::LostObject>& lost_objects,
    std::vector<int>& items_to_remove);

void UpdateDogsPositionAndGather(
    ::model::Map& map, std::chrono::milliseconds delta_time,
    std::unordered_map<int, ::model::LostObject>& lost_objects,
    std::chrono::milliseconds /*current_game_time*/,
    std::chrono::milliseconds /*retirement_time*/,
    std::unordered_map<
        ::model::Dog::Id, DogInactivityInfo,
        util::TaggedHasher<::model::Dog::Id>>& /*inactivity_info*/,
    std::vector<std::shared_ptr<::model::Player>>& /*players_to_retire*/);

}  // namespace game_state