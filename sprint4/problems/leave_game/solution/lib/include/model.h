#pragma once

#include <algorithm>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "loot_generator.h"
#include "tagged.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
  double x, y;
};

struct Size {
  Dimension width, height;
};

struct Rectangle {
  Point position;
  Size size;
};

struct Offset {
  Dimension dx, dy;
};

class Road {
  struct HorizontalTag {
    explicit HorizontalTag() = default;
  };

  struct VerticalTag {
    explicit VerticalTag() = default;
  };

 public:
  constexpr static HorizontalTag HORIZONTAL{};
  constexpr static VerticalTag VERTICAL{};

  Road(HorizontalTag, Point start, double end_x) noexcept
      : start_{start}, end_{end_x, start.y} {}

  Road(VerticalTag, Point start, double end_y) noexcept
      : start_{start}, end_{start.x, end_y} {}

  bool IsHorizontal() const noexcept { return start_.y == end_.y; }

  bool IsVertical() const noexcept { return start_.x == end_.x; }

  Point GetStart() const noexcept { return start_; }

  Point GetEnd() const noexcept { return end_; }

 private:
  Point start_;
  Point end_;
};

class Building {
 public:
  explicit Building(Rectangle bounds) noexcept : bounds_{bounds} {}

  const Rectangle& GetBounds() const noexcept { return bounds_; }

 private:
  Rectangle bounds_;
};

class Office {
 public:
  using Id = util::Tagged<std::string, Office>;

  Office(Id id, Point position, Offset offset) noexcept
      : id_{std::move(id)}, position_{position}, offset_{offset} {}

  const Id& GetId() const noexcept { return id_; }

  Point GetPosition() const noexcept { return position_; }

  Offset GetOffset() const noexcept { return offset_; }

 private:
  Id id_;
  Point position_;
  Offset offset_;
};

struct LostObject {
  int id;
  int type;
  Point position;
};

class Dog {
 public:
  using Id = util::Tagged<int, struct DogIdTag>;

  enum class Direction { NORTH, SOUTH, WEST, EAST };

  Dog(Id id, std::string name, Point position) noexcept
      : id_(id),
        name_(std::move(name)),
        position_(position),
        speed_x_(0.0),
        speed_y_(0.0),
        direction_(Direction::NORTH),
        score_(0) {}

  const Id& GetId() const noexcept { return id_; }
  const std::string& GetName() const noexcept { return name_; }
  const Point& GetPosition() const noexcept { return position_; }
  void SetPosition(Point position) noexcept { position_ = position; }

  double GetSpeedX() const noexcept { return speed_x_; }
  double GetSpeedY() const noexcept { return speed_y_; }
  void SetSpeed(double vx, double vy) noexcept {
    speed_x_ = vx;
    speed_y_ = vy;
  }

  Direction GetDirection() const noexcept { return direction_; }
  void SetDirection(Direction dir) noexcept { direction_ = dir; }

  std::string DirectionToString() const {
    switch (direction_) {
      case Direction::NORTH:
        return "U";
      case Direction::SOUTH:
        return "D";
      case Direction::WEST:
        return "L";
      case Direction::EAST:
        return "R";
    }
    return "U";
  }

  const std::vector<LostObject>& GetBag() const noexcept { return bag_; }
  std::vector<LostObject>& GetBagMutable() noexcept { return bag_; }
  void ClearBag() noexcept { bag_.clear(); }
  void AddToBag(const LostObject& obj) { bag_.push_back(obj); }
  size_t GetBagSize() const noexcept { return bag_.size(); }
  bool IsBagFull(size_t capacity) const noexcept {
    return capacity > 0 && bag_.size() >= capacity;
  }

  int GetScore() const noexcept { return score_; }
  void AddScore(int value) noexcept { score_ += value; }
  void SetScore(int score) noexcept { score_ = score; }

 private:
  Id id_;
  std::string name_;
  Point position_;
  double speed_x_;
  double speed_y_;
  Direction direction_;
  std::vector<LostObject> bag_;
  int score_ = 0;
};

class Map {
 public:
  using Id = util::Tagged<std::string, Map>;
  using Roads = std::vector<Road>;
  using Buildings = std::vector<Building>;
  using Offices = std::vector<Office>;
  using Dogs = std::vector<Dog>;

  Map(Id id, std::string name) noexcept
      : id_(std::move(id)),
        name_(std::move(name)),
        dog_speed_(1.0),
        loot_types_count_(0),
        bag_capacity_(3) {}

  const Id& GetId() const noexcept { return id_; }

  const std::string& GetName() const noexcept { return name_; }

  const Buildings& GetBuildings() const noexcept { return buildings_; }

  const Roads& GetRoads() const noexcept { return roads_; }

  const Offices& GetOffices() const noexcept { return offices_; }

  const Dogs& GetDogs() const noexcept { return dogs_; }  

  Dogs& GetDogsMutable() noexcept { return dogs_; }

  void AddRoad(const Road& road) { roads_.emplace_back(road); }

  void AddBuilding(const Building& building) {
    buildings_.emplace_back(building);
  }

  void AddOffice(Office office);

  void AddDog(Dog dog) { dogs_.push_back(std::move(dog)); }

  void RemoveDog(Dog::Id id) {
    dogs_.erase(std::remove_if(dogs_.begin(), dogs_.end(),
                               [id](const Dog& dog) { return dog.GetId() == id; }),
                dogs_.end());
  }

  Dog* FindDog(Dog::Id id) {
    for (auto& dog : dogs_) {
      if (dog.GetId() == id) {
        return &dog;
      }
    }
    return nullptr;
  }

  void SetDogSpeed(double speed) noexcept { dog_speed_ = speed; }
  double GetDogSpeed() const noexcept { return dog_speed_; }

  void SetLootTypesCount(size_t count) noexcept { loot_types_count_ = count; }
  size_t GetLootTypesCount() const noexcept { return loot_types_count_; }

  void SetBagCapacity(size_t capacity) noexcept { bag_capacity_ = capacity; }
  size_t GetBagCapacity() const noexcept { return bag_capacity_; }

  int GetItemValue(int type) const {
    auto it = loot_values_.find(type);
    return (it != loot_values_.end()) ? it->second : 10;
  }

  void SetLootValue(int type, int value) { loot_values_[type] = value; }

 private:
  using OfficeIdToIndex =
      std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

  Id id_;
  std::string name_;
  Roads roads_;
  Buildings buildings_;
  Dogs dogs_;
  double dog_speed_;
  size_t loot_types_count_;
  size_t bag_capacity_;
  std::unordered_map<int, int> loot_values_;

  OfficeIdToIndex warehouse_id_to_index_;
  Offices offices_;
};

class Game {
 public:
  using Maps = std::vector<Map>;

  int GetNextLootId() const noexcept { return next_loot_id_; }
  void SetNextLootId(int id) noexcept { next_loot_id_ = id; }

  const std::unordered_map<int, LostObject>& GetLostObjects() const noexcept {
    return lost_objects_;
  }

  void ClearDogs() {
    for (auto& map : maps_) {
      map.GetDogsMutable().clear();
    }
  }

  std::chrono::milliseconds GetGameTime() const noexcept { return game_time_; }
  void AddGameTime(std::chrono::milliseconds delta) { game_time_ += delta; }
  void SetGameTime(std::chrono::milliseconds time) { game_time_ = time; }

  void AddMap(Map map);

  Maps& GetMaps() noexcept { return maps_; }
  const Maps& GetMaps() const noexcept { return maps_; }

  const Map* FindMap(const Map::Id& id) const noexcept {
    if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
      return &maps_.at(it->second);
    }
    return nullptr;
  }

  Map* FindMap(const Map::Id& id) noexcept {
    if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
      return &maps_.at(it->second);
    }
    return nullptr;
  }

  void SetDefaultDogSpeed(double speed) noexcept { default_dog_speed_ = speed; }
  double GetDefaultDogSpeed() const noexcept { return default_dog_speed_; }

  void UpdateTime(std::chrono::milliseconds time_delta);

  std::unordered_map<int, LostObject>& GetLostObjectsMutable() {
    return lost_objects_;
  }

  void SetLootGeneratorConfig(std::chrono::milliseconds base_interval,
                              double probability) {
    loot_generator_ = loot_gen::LootGenerator(base_interval, probability);
  }

 private:
  void GenerateLoot(Map& map, unsigned count);
  Point GetRandomPointOnRoad(const Road& road) const;

  using MapIdHasher = util::TaggedHasher<Map::Id>;
  using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

  std::vector<Map> maps_;
  MapIdToIndex map_id_to_index_;
  double default_dog_speed_ = 1.0;

  std::unordered_map<int, LostObject> lost_objects_;
  int next_loot_id_ = 0;

  loot_gen::LootGenerator loot_generator_{std::chrono::milliseconds(1000), 0.5};

  std::chrono::milliseconds game_time_ = std::chrono::milliseconds::zero();
};

}  // namespace model