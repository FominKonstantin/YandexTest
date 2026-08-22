#include "record_manager.h"

#include <iostream>
#include <stdexcept>

namespace records {

RecordManager::RecordManager(const std::string& db_url) {
  try {
    connection_ = std::make_unique<pqxx::connection>(db_url);
    if (!connection_->is_open()) {
      throw std::runtime_error("Failed to connect to database");
    }
    std::cout << "Connected to PostgreSQL successfully" << std::endl;
  } catch (const std::exception& e) {
    throw std::runtime_error("Database connection error: " +
                             std::string(e.what()));
  }
}

void RecordManager::InitializeTable() {
  try {
    pqxx::work txn(*connection_);

    // Проверяем существование таблицы
    pqxx::result res = txn.exec(
        "SELECT EXISTS ("
        "  SELECT FROM information_schema.tables "
        "  WHERE table_name = 'retired_players'"
        ")");

    bool table_exists = res[0][0].as<bool>();

    if (!table_exists) {
      txn.exec(R"(
                CREATE TABLE retired_players (
                    id SERIAL PRIMARY KEY,
                    dog_name TEXT NOT NULL,
                    score INTEGER NOT NULL,
                    play_time_seconds DOUBLE PRECISION NOT NULL,
                    retired_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                )
            )");

      // Индексы для быстрой сортировки
      txn.exec(
          "CREATE INDEX idx_retired_score_desc ON retired_players (score "
          "DESC)");
      txn.exec(
          "CREATE INDEX idx_retired_play_time_asc ON retired_players "
          "(play_time_seconds ASC)");
      txn.exec(
          "CREATE INDEX idx_retired_name_asc ON retired_players (dog_name "
          "ASC)");

      std::cout << "Table 'retired_players' created successfully" << std::endl;
    } else {
      std::cout << "Table 'retired_players' already exists" << std::endl;
    }

    txn.commit();
  } catch (const std::exception& e) {
    throw std::runtime_error("Failed to initialize table: " +
                             std::string(e.what()));
  }
}

void RecordManager::AddRecord(const std::string& dog_name, int score,
                              double play_time_seconds) {
  try {
    pqxx::work txn(*connection_);
    txn.exec_params(
        "INSERT INTO retired_players (dog_name, score, play_time_seconds) "
        "VALUES ($1, $2, $3)",
        dog_name, score, play_time_seconds);
    txn.commit();
    std::cout << "Record saved: " << dog_name << " score=" << score
              << " time=" << play_time_seconds << "s" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Failed to save record: " << e.what() << std::endl;
    throw;
  }
}

std::vector<RecordEntry> RecordManager::GetRecords(int start,
                                                   int max_items) const {
  std::vector<RecordEntry> result;

  if (max_items <= 0) {
    return result;
  }

  // Максимальное количество записей - 100
  int limit = std::min(max_items, 100);

  try {
    pqxx::work txn(*connection_);

    std::string query = R"(
            SELECT dog_name, score, play_time_seconds
            FROM retired_players
            ORDER BY score DESC, play_time_seconds ASC, dog_name ASC
            LIMIT $1 OFFSET $2
        )";

    pqxx::result res = txn.exec_params(query, limit, start);

    result.reserve(res.size());
    for (const auto& row : res) {
      RecordEntry entry;
      entry.name = row["dog_name"].as<std::string>();
      entry.score = row["score"].as<int>();
      entry.play_time_seconds = row["play_time_seconds"].as<double>();
      result.push_back(entry);
    }

    txn.commit();
  } catch (const std::exception& e) {
    std::cerr << "Failed to get records: " << e.what() << std::endl;
    throw;
  }

  return result;
}

size_t RecordManager::GetTotalCount() const {
  try {
    pqxx::work txn(*connection_);
    pqxx::result res = txn.exec("SELECT COUNT(*) FROM retired_players");
    txn.commit();
    return res[0][0].as<size_t>();
  } catch (const std::exception& e) {
    std::cerr << "Failed to get total count: " << e.what() << std::endl;
    return 0;
  }
}

}  // namespace records