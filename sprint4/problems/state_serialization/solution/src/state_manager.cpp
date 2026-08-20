#include "state_manager.h"

#include <boost/json.hpp>
#include <fstream>
#include <iostream>

#include "state_serialization.h"

namespace state_manager {

void StateManager::Save(const model::Game& game, const model::Players& players,
                        const StateFile& path) {
  // Создаем временный файл
  StateFile temp_path = path;
  temp_path += ".tmp";

  try {
    // Сериализуем состояние
    auto game_state = state_serialization::ToGameState(game, players);
    auto json = state_serialization::SerializeGameState(game_state);

    // Записываем во временный файл
    std::ofstream file(temp_path);
    if (!file.is_open()) {
      throw std::runtime_error("Cannot open temporary file for writing: " +
                               temp_path.string());
    }
    file << boost::json::serialize(json);
    file.close();

    // Атомарно переименовываем
    std::filesystem::rename(temp_path, path);

    // Обновляем время последнего сохранения
    last_save_time_ = std::chrono::milliseconds(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());

  } catch (const std::exception& e) {
    std::cerr << "Error saving state to " << path << ": " << e.what()
              << std::endl;
    // Если временный файл остался, удаляем его
    if (std::filesystem::exists(temp_path)) {
      std::filesystem::remove(temp_path);
    }
    throw;
  }
}

bool StateManager::Load(model::Game& game, model::Players& players,
                        const StateFile& path) {
  if (!std::filesystem::exists(path)) {
    return false;
  }

  try {
    std::ifstream file(path);
    if (!file.is_open()) {
      throw std::runtime_error("Cannot open state file: " + path.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    auto json = boost::json::parse(buffer.str());
    auto game_state = state_serialization::DeserializeGameState(json);
    state_serialization::RestoreGameState(game, players, game_state);

    // Обновляем время последнего сохранения
    last_save_time_ = std::chrono::milliseconds(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());

    return true;

  } catch (const std::exception& e) {
    std::cerr << "Error loading state from " << path << ": " << e.what()
              << std::endl;
    throw;
  }
}

bool StateManager::ShouldSave(std::chrono::milliseconds game_time,
                              std::chrono::milliseconds save_period,
                              std::chrono::milliseconds last_save_time) {
  if (save_period <= std::chrono::milliseconds::zero()) {
    return false;
  }
  // Проверяем, прошло ли достаточно времени с последнего сохранения
  if (game_time < last_save_time) {
    // Игровое время сбросилось или изменилось, считаем что сохранение нужно
    return true;
  }
  return (game_time - last_save_time) >= save_period;
}

}  // namespace state_manager