#pragma once

#include <chrono>
#include <optional>
#include <unordered_map>
#include <vector>

#include "model.h"

namespace game_state {

constexpr double ROAD_HALF_WIDTH = 0.4;

void UpdateDogsPosition(model::Map& map, std::chrono::milliseconds delta_time);

void MoveDogOnRoad(model::Dog& dog, const model::Map& map,
                   std::chrono::milliseconds delta_time);

std::optional<model::Road> FindRoadContainingPoint(
    const model::Map& map, double x, double y, bool prefer_vertical = false);

double ClampPosition(double value, double min, double max);

void ProcessGathering(model::Map& map, double dt,
                      std::unordered_map<int, model::LostObject>& lost_objects,
                      std::vector<int>& items_to_remove);

void UpdateDogsPositionAndGather(
    model::Map& map, std::chrono::milliseconds delta_time,
    std::unordered_map<int, model::LostObject>& lost_objects);

}  // namespace game_state