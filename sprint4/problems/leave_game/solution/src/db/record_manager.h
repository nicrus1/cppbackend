#pragma once

#include <pqxx/pqxx>
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include "db_connection_pool.h"

namespace db {

struct Record {
    std::string name;
    int score;
    double play_time; // в секундах
};

class RecordManager {
public:
    explicit RecordManager(std::shared_ptr<ConnectionPool> pool);
    
    void InitTable();
    void AddRecord(const std::string& name, int score, double play_time);
    // max_items может быть 0, в этом случае возвращается пустой список
    std::vector<Record> GetRecords(int start, int max_items);
    
private:
    std::shared_ptr<ConnectionPool> pool_;
};

} // namespace db