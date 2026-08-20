#pragma once

#include <boost/json.hpp>
#include <filesystem>

#include "model.h"

namespace json_loader {

model::Game LoadGame(const std::filesystem::path& json_path);

boost::json::array GetLootTypesForMap(const std::filesystem::path& json_path,
                                      const std::string& map_id);

struct LootGeneratorConfig {
  std::chrono::milliseconds period;
  double probability;
};

LootGeneratorConfig ParseLootGeneratorConfig(const boost::json::value& config);

}  // namespace json_loader