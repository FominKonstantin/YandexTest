#pragma once
#include <boost/json.hpp>
#include <boost/json/value.hpp>

#include "model.h"
#include <cmath>

namespace http_handler {
namespace serializers {

boost::json::array SerializeRoads(const model::Map& map);
boost::json::array SerializeBuildings(const model::Map& map);
boost::json::array SerializeOffices(const model::Map& map);

inline boost::json::object SerializeMapInfo(const model::Map& map) {
  boost::json::object map_obj;
  map_obj["id"] = *map.GetId();
  map_obj["name"] = map.GetName();
  map_obj["roads"] = SerializeRoads(map);
  map_obj["buildings"] = SerializeBuildings(map);
  map_obj["offices"] = SerializeOffices(map);

  return map_obj;
}

inline boost::json::array SerializeRoads(const model::Map& map) {
  boost::json::array roads_array;
  for (const auto& road : map.GetRoads()) {
    boost::json::object road_obj;
    if (road.IsHorizontal()) {
      double x0 = road.GetStart().x;
      double y0 = road.GetStart().y;
      double x1 = road.GetEnd().x;
      if (std::floor(x0) == x0)
        road_obj["x0"] = static_cast<int64_t>(x0);
      else
        road_obj["x0"] = x0;
      if (std::floor(y0) == y0)
        road_obj["y0"] = static_cast<int64_t>(y0);
      else
        road_obj["y0"] = y0;
      if (std::floor(x1) == x1)
        road_obj["x1"] = static_cast<int64_t>(x1);
      else
        road_obj["x1"] = x1;
    } else {
      double x0 = road.GetStart().x;
      double y0 = road.GetStart().y;
      double y1 = road.GetEnd().y;
      if (std::floor(x0) == x0)
        road_obj["x0"] = static_cast<int64_t>(x0);
      else
        road_obj["x0"] = x0;
      if (std::floor(y0) == y0)
        road_obj["y0"] = static_cast<int64_t>(y0);
      else
        road_obj["y0"] = y0;
      if (std::floor(y1) == y1)
        road_obj["y1"] = static_cast<int64_t>(y1);
      else
        road_obj["y1"] = y1;
    }
    roads_array.push_back(road_obj);
  }
  return roads_array;
}

inline boost::json::array SerializeBuildings(const model::Map& map) {
  boost::json::array buildings_array;
  for (const auto& building : map.GetBuildings()) {
    boost::json::object building_obj;
    double x = building.GetBounds().position.x;
    double y = building.GetBounds().position.y;
    int w = building.GetBounds().size.width;
    int h = building.GetBounds().size.height;

    if (std::floor(x) == x)
      building_obj["x"] = static_cast<int64_t>(x);
    else
      building_obj["x"] = x;
    if (std::floor(y) == y)
      building_obj["y"] = static_cast<int64_t>(y);
    else
      building_obj["y"] = y;
    building_obj["w"] = w;
    building_obj["h"] = h;
    buildings_array.push_back(building_obj);
  }
  return buildings_array;
}

inline boost::json::array SerializeOffices(const model::Map& map) {
  boost::json::array offices_array;
  for (const auto& office : map.GetOffices()) {
    boost::json::object office_obj;
    office_obj["id"] = *office.GetId();
    double x = office.GetPosition().x;
    double y = office.GetPosition().y;
    if (std::floor(x) == x)
      office_obj["x"] = static_cast<int64_t>(x);
    else
      office_obj["x"] = x;
    if (std::floor(y) == y)
      office_obj["y"] = static_cast<int64_t>(y);
    else
      office_obj["y"] = y;
    office_obj["offsetX"] = office.GetOffset().dx;
    office_obj["offsetY"] = office.GetOffset().dy;
    offices_array.push_back(office_obj);
  }
  return offices_array;
}

}  // namespace serializers
}  // namespace http_handler