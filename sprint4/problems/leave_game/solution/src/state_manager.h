#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>

#include "model.h"
#include "players.h"
#include "state_serialization.h"  // Мы создадим этот файл позже

namespace state_manager {

using StateFile = std::filesystem::path;

struct SaveStateConfig {
  StateFile file_path;
  std::chrono::milliseconds save_period = std::chrono::milliseconds::zero();
};

class StateManager {
 public:
  StateManager() = default;

  // Сохраняет состояние игры в файл
  void Save(const model::Game& game, const model::Players& players,
            const StateFile& path);

  // Восстанавливает состояние игры из файла
  // Возвращает true, если состояние было успешно восстановлено
  bool Load(model::Game& game, model::Players& players, const StateFile& path);

  // Сохраняет состояние с использованием атомарного переименования
  bool SaveAtomic(const model::Game& game, const model::Players& players,
                  const StateFile& path);

  // Проверяет, нужно ли выполнить автоматическое сохранение
  bool ShouldSave(std::chrono::milliseconds game_time,
                  std::chrono::milliseconds save_period,
                  std::chrono::milliseconds last_save_time);

 private:
  std::chrono::milliseconds last_save_time_ = std::chrono::milliseconds::zero();
};

}  // namespace state_manager