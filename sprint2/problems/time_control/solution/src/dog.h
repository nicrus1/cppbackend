#pragma once
#include <cstdint>
#include <string>

namespace model {

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
    Dog(uint64_t id, Position pos, Speed spd = {0.0, 0.0}, Direction dir = Direction::NORTH)
        : id_(id), pos_(pos), speed_(spd), dir_(dir) {}

    uint64_t GetId() const { return id_; }
    const Position& GetPosition() const { return pos_; }
    const Speed& GetSpeed() const { return speed_; }
    Direction GetDirection() const { return dir_; }

    void SetPosition(Position pos) { pos_ = pos; }
    void SetSpeed(Speed spd) { speed_ = spd; }
    void SetDirection(Direction dir) { dir_ = dir; }
    
    void SetSpeedFromDirection(Direction dir, double speed) {
    switch (dir) {
        case Direction::NORTH:
            speed_ = {0.0, -speed};
            break;
        case Direction::SOUTH:
            speed_ = {0.0, speed};
            break;
        case Direction::WEST:
            speed_ = {-speed, 0.0};
            break;
        case Direction::EAST:
            speed_ = {speed, 0.0};
            break;
    }
    dir_ = dir;
}
    
    void Stop() {
        speed_ = {0.0, 0.0};
    }

private:
    uint64_t id_;
    Position pos_;
    Speed speed_;
    Direction dir_;
};

} // namespace model