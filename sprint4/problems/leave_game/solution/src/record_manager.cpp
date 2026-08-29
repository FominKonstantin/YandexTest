#include "record_manager.h"

#include <pqxx/pqxx>

#include <algorithm>
#include <stdexcept>

namespace records {

RecordManager::RecordManager(const std::string& db_url) : db_url_(db_url) {
  pqxx::connection connection(db_url_);
  if (!connection.is_open()) {
    throw std::runtime_error("Failed to connect to database");
  }
}

void RecordManager::InitializeTable() {
  pqxx::connection connection(db_url_);
  pqxx::work txn(connection);
  txn.exec(R"(
      CREATE TABLE IF NOT EXISTS retired_players (
          id SERIAL PRIMARY KEY,
          dog_name TEXT NOT NULL,
          score INTEGER NOT NULL,
          play_time_seconds DOUBLE PRECISION NOT NULL
      )
  )");
  txn.exec(R"(
      CREATE INDEX IF NOT EXISTS idx_retired_players_order
      ON retired_players (score DESC, play_time_seconds ASC, dog_name ASC)
  )");
  txn.commit();
}

void RecordManager::AddRecord(const std::string& dog_name, int score,
                              double play_time_seconds) {
  pqxx::connection connection(db_url_);
  pqxx::work txn(connection);
  txn.exec_params(
      "INSERT INTO retired_players (dog_name, score, play_time_seconds) "
      "VALUES ($1, $2, $3)",
      dog_name, score, play_time_seconds);
  txn.commit();
}

std::vector<RecordEntry> RecordManager::GetRecords(int start,
                                                   int max_items) const {
  if (start < 0 || max_items <= 0) {
    return {};
  }

  pqxx::connection connection(db_url_);
  pqxx::read_transaction txn(connection);
  const auto res = txn.exec_params(
      R"(
          SELECT dog_name, score, play_time_seconds
          FROM retired_players
          ORDER BY score DESC, play_time_seconds ASC, dog_name ASC
          LIMIT $1 OFFSET $2
      )",
      std::min(max_items, 100), start);

  std::vector<RecordEntry> result;
  result.reserve(res.size());
  for (const auto& row : res) {
    result.push_back({row["dog_name"].as<std::string>(), row["score"].as<int>(),
                      row["play_time_seconds"].as<double>()});
  }
  return result;
}

size_t RecordManager::GetTotalCount() const {
  pqxx::connection connection(db_url_);
  pqxx::read_transaction txn(connection);
  const auto res = txn.exec("SELECT COUNT(*) FROM retired_players");
  return res[0][0].as<size_t>();
}

}  // namespace records
