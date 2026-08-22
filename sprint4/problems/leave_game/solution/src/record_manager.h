#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <pqxx/pqxx>
#include <string>
#include <vector>

namespace records {

struct RecordEntry {
  std::string name;
  int score;
  double play_time_seconds;  // время в секундах с плавающей точкой
};

class RecordManager {
 public:
  explicit RecordManager(const std::string& db_url);

  void InitializeTable();

  void AddRecord(const std::string& dog_name, int score,
                 double play_time_seconds);

  std::vector<RecordEntry> GetRecords(int start, int max_items) const;

  size_t GetTotalCount() const;

 private:
  std::unique_ptr<pqxx::connection> connection_;
};

}  // namespace records