#include "json_loader.h"

#include <boost/json.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace json_loader {

static std::string ReadFile(const std::filesystem::path& json_path) {
  std::ifstream file(json_path);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open config file: " +
                             json_path.string());
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

static model::Road LoadRoad(const boost::json::object& road_obj) {
  try {
    if (road_obj.contains("x0") && road_obj.contains("y0") &&
        road_obj.contains("x1")) {
      double x0 = static_cast<double>(road_obj.at("x0").as_int64());
      double y0 = static_cast<double>(road_obj.at("y0").as_int64());
      double x1 = static_cast<double>(road_obj.at("x1").as_int64());
      return model::Road(model::Road::HORIZONTAL, {x0, y0}, x1);
    } else if (road_obj.contains("x0") && road_obj.contains("y0") &&
               road_obj.contains("y1")) {
      double x0 = static_cast<double>(road_obj.at("x0").as_int64());
      double y0 = static_cast<double>(road_obj.at("y0").as_int64());
      double y1 = static_cast<double>(road_obj.at("y1").as_int64());
      return model::Road(model::Road::VERTICAL, {x0, y0}, y1);
    }
    throw std::runtime_error("Invalid road definition");
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("Failed to load road: ") + e.what());
  }
}

static model::Building LoadBuilding(const boost::json::object& building_obj) {
  try {
    double x = static_cast<double>(building_obj.at("x").as_int64());
    double y = static_cast<double>(building_obj.at("y").as_int64());
    model::Dimension w = building_obj.at("w").as_int64();
    model::Dimension h = building_obj.at("h").as_int64();
    model::Rectangle rect{{x, y}, {w, h}};
    return model::Building(rect);
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("Failed to load building: ") +
                             e.what());
  }
}

static model::Office LoadOffice(const boost::json::object& office_obj) {
  try {
    std::string office_id = office_obj.at("id").as_string().c_str();
    double x = static_cast<double>(office_obj.at("x").as_int64());
    double y = static_cast<double>(office_obj.at("y").as_int64());
    model::Dimension offset_x = office_obj.at("offsetX").as_int64();
    model::Dimension offset_y = office_obj.at("offsetY").as_int64();
    return model::Office(model::Office::Id(office_id), {x, y},
                         {offset_x, offset_y});
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("Failed to load office: ") + e.what());
  }
}

static model::Map LoadMap(const boost::json::object& map_obj,
                          double default_speed, size_t default_bag_capacity) {
  try {
    std::string id = map_obj.at("id").as_string().c_str();
    std::string name = map_obj.at("name").as_string().c_str();
    model::Map map(model::Map::Id(id), name);

    if (map_obj.contains("dogSpeed")) {
      map.SetDogSpeed(map_obj.at("dogSpeed").as_double());
    } else {
      map.SetDogSpeed(default_speed);
    }

    size_t bag_capacity = default_bag_capacity;
    if (map_obj.contains("bagCapacity")) {
      bag_capacity = static_cast<size_t>(map_obj.at("bagCapacity").as_int64());
    }
    map.SetBagCapacity(bag_capacity);

    if (map_obj.contains("roads")) {
      const auto& roads_array = map_obj.at("roads").as_array();
      for (const auto& road_value : roads_array) {
        map.AddRoad(LoadRoad(road_value.as_object()));
      }
    }

    if (map_obj.contains("buildings")) {
      const auto& buildings_array = map_obj.at("buildings").as_array();
      for (const auto& building_value : buildings_array) {
        map.AddBuilding(LoadBuilding(building_value.as_object()));
      }
    }

    if (map_obj.contains("offices")) {
      const auto& offices_array = map_obj.at("offices").as_array();
      for (const auto& office_value : offices_array) {
        map.AddOffice(LoadOffice(office_value.as_object()));
      }
    }

    if (map_obj.contains("lootTypes")) {
      const auto& loot_types = map_obj.at("lootTypes").as_array();
      map.SetLootTypesCount(loot_types.size());

      for (const auto& item : loot_types) {
        const auto& obj = item.as_object();
        if (obj.contains("type") && obj.contains("value")) {
          int type = obj.at("type").as_int64();
          int value = obj.at("value").as_int64();
          map.SetLootValue(type, value);
        }
      }
    }

    return map;
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("Failed to load map: ") + e.what());
  }
}

model::Game LoadGame(const std::filesystem::path& json_path) {
  try {
    std::string json_str = ReadFile(json_path);

    boost::json::value json_value;
    try {
      json_value = boost::json::parse(json_str);
    } catch (const boost::system::system_error& e) {
      throw std::runtime_error(std::string("JSON parse error: ") + e.what());
    }

    boost::json::object json_obj = json_value.as_object();

    if (!json_obj.contains("maps")) {
      throw std::runtime_error("Config file missing 'maps' field");
    }

    const auto& maps_array = json_obj.at("maps").as_array();
    model::Game game;

    if (json_obj.contains("defaultDogSpeed")) {
      game.SetDefaultDogSpeed(json_obj.at("defaultDogSpeed").as_double());
    }

    size_t default_bag_capacity = 3;
    if (json_obj.contains("defaultBagCapacity")) {
      default_bag_capacity =
          static_cast<size_t>(json_obj.at("defaultBagCapacity").as_int64());
    }

    if (json_obj.contains("lootGeneratorConfig")) {
      auto loot_config = ParseLootGeneratorConfig(json_value);
      game.SetLootGeneratorConfig(loot_config.period, loot_config.probability);
    }

    for (const auto& map_value : maps_array) {
      auto map = LoadMap(map_value.as_object(), game.GetDefaultDogSpeed(),
                         default_bag_capacity);
      game.AddMap(std::move(map));
    }

    return game;
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("Failed to load game config: ") +
                             e.what());
  }
}

boost::json::array GetLootTypesForMap(const std::filesystem::path& json_path,
                                      const std::string& map_id) {
  std::string json_str = ReadFile(json_path);
  auto json_value = boost::json::parse(json_str);
  auto json_obj = json_value.as_object();

  if (!json_obj.contains("maps")) {
    return {};
  }

  const auto& maps = json_obj.at("maps").as_array();
  for (const auto& map_val : maps) {
    const auto& map_obj = map_val.as_object();
    if (map_obj.at("id").as_string() == map_id) {
      if (map_obj.contains("lootTypes")) {
        return map_obj.at("lootTypes").as_array();
      }
      return {};
    }
  }
  return {};
}

LootGeneratorConfig ParseLootGeneratorConfig(const boost::json::value& config) {
  const auto& obj = config.as_object();
  const auto& loot_gen = obj.at("lootGeneratorConfig").as_object();

  LootGeneratorConfig result;
  result.period = std::chrono::milliseconds(
      static_cast<int64_t>(loot_gen.at("period").as_double() * 1000));
  result.probability = loot_gen.at("probability").as_double();

  return result;
}

}  // namespace json_loader