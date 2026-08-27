#include "model.h"

#include <random>
#include <stdexcept>

namespace model {
using namespace std::literals;

void Map::AddOffice(Office office) {
  if (warehouse_id_to_index_.contains(office.GetId())) {
    throw std::invalid_argument("Duplicate warehouse");
  }

  const size_t index = offices_.size();
  Office& o = offices_.emplace_back(std::move(office));
  try {
    warehouse_id_to_index_.emplace(o.GetId(), index);
  } catch (...) {
    offices_.pop_back();
    throw;
  }
}

void Game::AddMap(Map map) {
  const size_t index = maps_.size();
  if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index);
      !inserted) {
    throw std::invalid_argument("Map with id "s + *map.GetId() +
                                " already exists"s);
  } else {
    try {
      maps_.emplace_back(std::move(map));
    } catch (...) {
      map_id_to_index_.erase(it);
      throw;
    }
  }
}

void Game::UpdateTime(std::chrono::milliseconds time_delta) {
  if (maps_.empty()) return;

  for (auto& map : maps_) {
    unsigned looters_count = map.GetDogs().size();
    unsigned current_loot = lost_objects_.size();

    unsigned new_loot_count =
        loot_generator_.Generate(time_delta, current_loot, looters_count);

    if (new_loot_count > 0) {
      GenerateLoot(map, new_loot_count);
    }
  }
}

void Game::GenerateLoot(Map& map, unsigned count) {
  static std::random_device rd;
  static std::mt19937 gen(rd());

  if (map.GetLootTypesCount() == 0) return;

  std::uniform_int_distribution<> type_dist(0, map.GetLootTypesCount() - 1);

  auto& roads = map.GetRoads();
  if (roads.empty()) return;

  std::uniform_int_distribution<> road_dist(0, roads.size() - 1);

  for (unsigned i = 0; i < count; ++i) {
    const auto& road = roads[road_dist(gen)];
    Point pos = GetRandomPointOnRoad(road);

    LostObject obj{
        .id = next_loot_id_++, .type = type_dist(gen), .position = pos};
    lost_objects_[obj.id] = obj;
  }
}

Point Game::GetRandomPointOnRoad(const Road& road) const {
  static std::random_device rd;
  static std::mt19937 gen(rd());

  std::uniform_real_distribution<double> dist(0.0, 1.0);

  double t = dist(gen);
  return {road.GetStart().x + (road.GetEnd().x - road.GetStart().x) * t,
          road.GetStart().y + (road.GetEnd().y - road.GetStart().y) * t};
}

}  // namespace model