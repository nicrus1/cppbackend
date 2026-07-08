#pragma once

#include <vector>
#include <string>
#include <mutex>

namespace db {

struct Record {
    std::string name;
    int score = 0;
    int play_time = 0;
};

class RecordManager {
public:
    void AddRecord(const std::string& name, int score, int play_time) {
        std::lock_guard<std::mutex> lock(mutex_);
        records_.push_back({name, score, play_time});
        // Сортировка по убыванию очков
        std::sort(records_.begin(), records_.end(), 
            [](const Record& a, const Record& b) {
                if (a.score != b.score) return a.score > b.score;
                return a.play_time < b.play_time;
            });
    }
    
    std::vector<Record> GetRecords(int start, int max_items) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Record> result;
        if (start >= static_cast<int>(records_.size())) {
            return result;
        }
        int end = std::min(start + max_items, static_cast<int>(records_.size()));
        for (int i = start; i < end; ++i) {
            result.push_back(records_[i]);
        }
        return result;
    }
    
private:
    std::vector<Record> records_;
    mutable std::mutex mutex_;
};

} // namespace db