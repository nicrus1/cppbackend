#include "record_manager.h"
#include <stdexcept>
#include <iostream>

namespace db {

RecordManager::RecordManager(std::shared_ptr<ConnectionPool> pool)
    : pool_(std::move(pool)) {
    if (!pool_) {
        throw std::runtime_error("RecordManager: connection pool is null");
    }
}

void RecordManager::InitTable() {
    try {
        auto conn = pool_->GetConnection();
        pqxx::work tx{*conn};
        
        tx.exec(R"(
            CREATE TABLE IF NOT EXISTS retired_players (
                id SERIAL PRIMARY KEY,
                name TEXT NOT NULL,
                score INTEGER NOT NULL,
                play_time DOUBLE PRECISION NOT NULL,
                retired_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            );
        )");
        
        tx.exec(R"(
            CREATE INDEX IF NOT EXISTS idx_retired_players_score_time_name 
            ON retired_players (score DESC, play_time ASC, name ASC);
        )");
        
        tx.commit();
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize records table: " << e.what() << std::endl;
        throw;
    }
}

void RecordManager::AddRecord(const std::string& name, int score, double play_time) {
    try {
        auto conn = pool_->GetConnection();
        pqxx::work tx{*conn};
        
        tx.exec_params(
            "INSERT INTO retired_players (name, score, play_time) VALUES ($1, $2, $3);",
            name, score, play_time
        );
        
        tx.commit();
    } catch (const std::exception& e) {
        std::cerr << "Failed to add record: " << e.what() << std::endl;
        throw;
    }
}

std::vector<Record> RecordManager::GetRecords(int start, int max_items) {
    if (start < 0) {
        start = 0;
    }
    if (max_items < 1 || max_items > 100) {
        throw std::runtime_error("max_items must be between 1 and 100");
    }
    
    try {
        auto conn = pool_->GetConnection();
        pqxx::read_transaction tx{*conn};
        
        auto result = tx.exec_params(
            "SELECT name, score, play_time FROM retired_players "
            "ORDER BY score DESC, play_time ASC, name ASC "
            "OFFSET $1 LIMIT $2;",
            start, max_items
        );
        
        std::vector<Record> records;
        records.reserve(result.size());
        
        // Используем индексный доступ вместо range-based for
        for (size_t i = 0; i < result.size(); ++i) {
            Record rec;
            rec.name = result[i][0].as<std::string>();
            rec.score = result[i][1].as<int>();
            rec.play_time = result[i][2].as<double>();
            records.push_back(std::move(rec));
        }
        
        return records;
    } catch (const std::exception& e) {
        std::cerr << "Failed to get records: " << e.what() << std::endl;
        throw;
    }
}

} // namespace db