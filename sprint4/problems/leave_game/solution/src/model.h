#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <chrono>

#include "tagged.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

struct Position {
    double x = 0.0;
    double y = 0.0;
};

struct Speed {
    double vx = 0.0;
    double vy = 0.0;
};

enum class Direction {
    NORTH,
    SOUTH,
    WEST,
    EAST
};

inline std::string DirectionToString(Direction dir) {
    switch (dir) {
        case Direction::NORTH: return "U";
        case Direction::SOUTH: return "D";
        case Direction::WEST:  return "L";
        case Direction::EAST:  return "R";
    }
    return "U";
}

inline Direction StringToDirection(const std::string& str) {
    if (str == "U") return Direction::NORTH;
    if (str == "D") return Direction::SOUTH;
    if (str == "L") return Direction::WEST;
    if (str == "R") return Direction::EAST;
    return Direction::NORTH;
}

class Dog {
public:
    using Id = uint64_t;

    Dog(Id id, Point pos, double speed)
        : id_(id)
        , pos_{static_cast<double>(pos.x), static_cast<double>(pos.y)}
        , default_speed_(speed)
        , last_activity_time_(std::chrono::steady_clock::now()) {
    }

    Dog(Id id, Position pos, double speed)
        : id_(id)
        , pos_(pos)
        , default_speed_(speed)
        , last_activity_time_(std::chrono::steady_clock::now()) {
    }

    Id GetId() const {
        return id_;
    }

    const Position& GetPosition() const {
        return pos_;
    }

    void SetPosition(Position pos) {
        pos_ = pos;
    }
    
    void SetPosition(double x, double y) {
        pos_ = {x, y};
    }

    const Speed& GetSpeed() const {
        return speed_;
    }

    void SetSpeed(Speed speed) {
        speed_ = speed;
        // Если скорость изменилась на ненулевую, обновляем время активности
        if (speed.vx != 0.0 || speed.vy != 0.0) {
            last_activity_time_ = std::chrono::steady_clock::now();
        }
    }

    Direction GetDirection() const {
        return direction_;
    }

    void SetDirection(Direction dir) {
        direction_ = dir;
    }

    double GetDefaultSpeed() const {
        return default_speed_;
    }

    // Методы для отслеживания бездействия
    void SetLastActivityTime(std::chrono::steady_clock::time_point time) {
        last_activity_time_ = time;
    }
    
    std::chrono::steady_clock::time_point GetLastActivityTime() const {
        return last_activity_time_;
    }
    
    void SetRetirementTime(std::chrono::milliseconds time) {
        retirement_time_ = time;
    }
    
    std::chrono::milliseconds GetRetirementTime() const {
        return retirement_time_;
    }
    
    void SetTotalPlayTime(std::chrono::milliseconds time) {
        total_play_time_ = time;
    }
    
    std::chrono::milliseconds GetTotalPlayTime() const {
        return total_play_time_;
    }
    
    // Методы для очков
    int GetScore() const {
        return score_;
    }
    
    void AddScore(int value) {
        score_ += value;
    }

private:
    Id id_;
    Position pos_;
    Speed speed_{0.0, 0.0};
    Direction direction_ = Direction::NORTH;
    double default_speed_;
    int score_ = 0;
    
    std::chrono::steady_clock::time_point last_activity_time_;
    std::chrono::milliseconds retirement_time_{60000}; // 1 минута по умолчанию
    std::chrono::milliseconds total_play_time_{0};
};

// ... остальной код (Road, Building, Office, Map, Game) без изменений ...

}  // namespace model