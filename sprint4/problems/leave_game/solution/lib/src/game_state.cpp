#include "game_state.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <limits>

#include "collision_detector.h"
#include "player.h"

using namespace model;

namespace game_state {

void UpdateDogsPositionAndGather(
    ::model::Map& map, std::chrono::milliseconds delta_time,
    std::unordered_map<int, ::model::LostObject>& lost_objects,
    std::chrono::milliseconds current_game_time,
    std::chrono::milliseconds /*retirement_time*/,
    std::unordered_map<
        ::model::Dog::Id, DogInactivityInfo,
        util::TaggedHasher<::model::Dog::Id>>& inactivity_info,
    std::vector<std::shared_ptr<::model::Player>>& /*players_to_retire*/) {
  const auto tick_start = current_game_time - delta_time;

  // Двигаем собак и фиксируем точный момент остановки у края дороги.
  for (auto& dog : map.GetDogsMutable()) {
    const auto old_pos = dog.GetPosition();
    const double old_vx = dog.GetSpeedX();
    const double old_vy = dog.GetSpeedY();
    const bool was_moving = old_vx != 0.0 || old_vy != 0.0;

    MoveDogOnRoad(dog, map, delta_time);

    const bool is_stopped = dog.GetSpeedX() == 0.0 && dog.GetSpeedY() == 0.0;
    if (was_moving && is_stopped) {
      const auto new_pos = dog.GetPosition();
      const double distance =
          std::hypot(new_pos.x - old_pos.x, new_pos.y - old_pos.y);
      const double speed = std::hypot(old_vx, old_vy);
      const double tick_seconds = delta_time.count() / 1000.0;
      const double movement_seconds =
          speed > 0.0 ? std::clamp(distance / speed, 0.0, tick_seconds) : 0.0;
      const auto movement_ms = std::chrono::milliseconds(
          static_cast<int64_t>(std::llround(movement_seconds * 1000.0)));

      auto& info = inactivity_info[dog.GetId()];
      info.is_idle = true;
      info.idle_start_time = tick_start + movement_ms;
    }
  }

  // Сбор предметов
  double dt = delta_time.count() / 1000.0;
  if (dt <= 0.0) return;

  std::vector<int> items_to_remove;
  ProcessGathering(map, dt, lost_objects, items_to_remove);

  for (int id : items_to_remove) {
    auto it = lost_objects.find(id);
    if (it != lost_objects.end()) {
      for (auto& dog : map.GetDogsMutable()) {
        auto& bag = dog.GetBagMutable();
        auto bag_it = std::find_if(
            bag.begin(), bag.end(),
            [id](const ::model::LostObject& obj) { return obj.id == id; });
        if (bag_it != bag.end()) {
          int value = map.GetItemValue(bag_it->type);
          dog.AddScore(value);
          break;
        }
      }
    }
    lost_objects.erase(id);
  }
}   

double ClampPosition(double value, double min, double max) {
  return std::max(min, std::min(max, value));
}

void MoveDogOnRoad(::model::Dog& dog, const ::model::Map& map,
                   std::chrono::milliseconds delta_time) {
  auto pos = dog.GetPosition();
  double vx = dog.GetSpeedX();
  double vy = dog.GetSpeedY();

  if (vx == 0.0 && vy == 0.0) {
    return;
  }

  // ===== ПРОВЕРКА НА ПУСТЫЕ ДОРОГИ =====
  const auto& roads = map.GetRoads();
  if (roads.empty()) {
    dog.SetSpeed(0, 0);
    return;
  }
  // ===== КОНЕЦ =====

  double cur_x = pos.x;
  double cur_y = pos.y;
  double dt = delta_time.count() / 1000.0;

  bool prefer_vertical = (std::abs(vy) >= std::abs(vx));

  auto current_road =
      FindRoadContainingPoint(map, cur_x, cur_y, prefer_vertical);

  if (!current_road) {
    const auto& roads = map.GetRoads();
    if (roads.empty()) {
      dog.SetSpeed(0, 0);
      return;
    }

    const ::model::Road* target_road = nullptr;
    for (const auto& road : roads) {
      if (prefer_vertical && road.IsVertical()) {
        target_road = &road;
        break;
      } else if (!prefer_vertical && road.IsHorizontal()) {
        target_road = &road;
        break;
      }
    }

    if (!target_road) {
      target_road = &roads[0];
    }

    if (target_road->IsVertical()) {
      dog.SetPosition({target_road->GetStart().x, cur_y});
    } else {
      dog.SetPosition({cur_x, target_road->GetStart().y});
    }

    current_road = FindRoadContainingPoint(
        map, dog.GetPosition().x, dog.GetPosition().y, prefer_vertical);
    if (!current_road) {
      return;
    }
  }

  if (current_road->IsHorizontal() && std::abs(vy) > std::abs(vx)) {
    auto vertical_road = FindRoadContainingPoint(map, cur_x, cur_y, true);
    if (vertical_road) {
      current_road = vertical_road;
    }
  }

  if (current_road->IsVertical() && std::abs(vx) > std::abs(vy)) {
    auto horizontal_road = FindRoadContainingPoint(map, cur_x, cur_y, false);
    if (horizontal_road) {
      current_road = horizontal_road;
    }
  }

  double new_x = cur_x + vx * dt;
  double new_y = cur_y + vy * dt;

  const double EPSILON = 0.001;

  if (current_road->IsHorizontal()) {
    double road_min_x =
        std::min(current_road->GetStart().x, current_road->GetEnd().x) -
        ROAD_HALF_WIDTH;
    double road_max_x =
        std::max(current_road->GetStart().x, current_road->GetEnd().x) +
        ROAD_HALF_WIDTH;
    double road_y = current_road->GetStart().y;

    if (vx > 0 && cur_x >= road_max_x - EPSILON) {
      new_x = road_max_x;
      dog.SetSpeed(0, 0);
    } else if (vx < 0 && cur_x <= road_min_x + EPSILON) {
      new_x = road_min_x;
      dog.SetSpeed(0, 0);
    } else if (vx > 0 && new_x >= road_max_x) {
      new_x = road_max_x;
      dog.SetSpeed(0, 0);
    } else if (vx < 0 && new_x <= road_min_x) {
      new_x = road_min_x;
      dog.SetSpeed(0, 0);
    }

    if (new_y < road_y - ROAD_HALF_WIDTH) {
      new_y = road_y - ROAD_HALF_WIDTH;
      if (vx == 0 && vy != 0) {
        dog.SetSpeed(0, 0);
      }
    } else if (new_y > road_y + ROAD_HALF_WIDTH) {
      new_y = road_y + ROAD_HALF_WIDTH;
      if (vx == 0 && vy != 0) {
        dog.SetSpeed(0, 0);
      }
    }

  } else {
    double road_min_y =
        std::min(current_road->GetStart().y, current_road->GetEnd().y) -
        ROAD_HALF_WIDTH;
    double road_max_y =
        std::max(current_road->GetStart().y, current_road->GetEnd().y) +
        ROAD_HALF_WIDTH;
    double road_x = current_road->GetStart().x;

    if (vy > 0 && cur_y >= road_max_y - EPSILON) {
      new_y = road_max_y;
      dog.SetSpeed(0, 0);
    } else if (vy < 0 && cur_y <= road_min_y + EPSILON) {
      new_y = road_min_y;
      dog.SetSpeed(0, 0);
    } else if (vy > 0 && new_y >= road_max_y) {
      new_y = road_max_y;
      dog.SetSpeed(0, 0);
    } else if (vy < 0 && new_y <= road_min_y) {
      new_y = road_min_y;
      dog.SetSpeed(0, 0);
    }

    if (new_x < road_x - ROAD_HALF_WIDTH) {
      new_x = road_x - ROAD_HALF_WIDTH;
      if (vy == 0 && vx != 0) {
        dog.SetSpeed(0, 0);
      }
    } else if (new_x > road_x + ROAD_HALF_WIDTH) {
      new_x = road_x + ROAD_HALF_WIDTH;
      if (vy == 0 && vx != 0) {
        dog.SetSpeed(0, 0);
      }
    }
  }

  dog.SetPosition({new_x, new_y});
}

void UpdateDogsPosition(::model::Map& map,
                        std::chrono::milliseconds delta_time) {
  auto& dogs = map.GetDogsMutable();
  for (auto& dog : dogs) {
    MoveDogOnRoad(dog, map, delta_time);
  }
}

class GatheringProvider : public collision_detector::ItemGathererProvider {
 public:
  GatheringProvider(const std::vector<::model::LostObject>& items,
                    const std::vector<::model::Dog*>& dogs, double dt)
      : items_(items), dogs_(dogs), dt_(dt) {}

  size_t ItemsCount() const override { return items_.size(); }

  collision_detector::Item GetItem(size_t idx) const override {
    const auto& obj = items_[idx];
    return {geom::Point2D{obj.position.x, obj.position.y}, 0.0};
  }

  size_t GatherersCount() const override { return dogs_.size(); }

  collision_detector::Gatherer GetGatherer(size_t idx) const override {
    const auto* dog = dogs_[idx];
    auto pos = dog->GetPosition();

    double end_x = pos.x + dog->GetSpeedX() * dt_;
    double end_y = pos.y + dog->GetSpeedY() * dt_;

    return {geom::Point2D{pos.x, pos.y}, geom::Point2D{end_x, end_y}, 0.6};
  }

 private:
  const std::vector<::model::LostObject>& items_;
  const std::vector<::model::Dog*>& dogs_;
  double dt_;
};

void ProcessGathering(
    ::model::Map& map, double dt,
    std::unordered_map<int, ::model::LostObject>& lost_objects,
    std::vector<int>& items_to_remove) {
  auto& dogs = map.GetDogsMutable();

  if (dogs.empty() || lost_objects.empty()) return;

  // ===== ОПТИМИЗАЦИЯ С РЕЗЕРВИРОВАНИЕМ ПАМЯТИ =====
  std::vector<::model::Dog*> dog_ptrs;
  dog_ptrs.reserve(dogs.size());
  for (auto& dog : dogs) {
    dog_ptrs.push_back(&dog);
  }

  std::vector<::model::LostObject> item_list;
  item_list.reserve(lost_objects.size());
  for (const auto& [id, obj] : lost_objects) {
    item_list.push_back(obj);
  }
  // ===== КОНЕЦ =====

  GatheringProvider provider(item_list, dog_ptrs, dt);

  auto events = collision_detector::FindGatherEvents(provider);

  if (events.empty()) return;

  std::sort(events.begin(), events.end(), [](const auto& a, const auto& b) {
    if (a.time != b.time) return a.time < b.time;
    return a.gatherer_id < b.gatherer_id;
  });

  std::vector<bool> item_collected(item_list.size(), false);
  size_t bag_capacity = map.GetBagCapacity();

  for (const auto& event : events) {
    size_t dog_idx = event.gatherer_id;
    size_t item_idx = event.item_id;

    if (dog_idx >= dog_ptrs.size() || item_idx >= item_list.size()) continue;
    if (item_collected[item_idx]) continue;

    auto* dog = dog_ptrs[dog_idx];
    if (!dog) continue;

    int item_id = item_list[item_idx].id;

    if (!dog->IsBagFull(bag_capacity)) {
      auto item_it = lost_objects.find(item_id);
      if (item_it != lost_objects.end()) {
        dog->AddToBag(item_it->second);
        items_to_remove.push_back(item_id);
        item_collected[item_idx] = true;
      }
    }
  }
}

}  // namespace game_state