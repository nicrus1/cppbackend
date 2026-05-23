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
    NORTH,  // "U"
    SOUTH,  // "D"
    WEST,   // "L"
    EAST    // "R"
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

private:
    uint64_t id_;
    Position pos_;
    Speed speed_;
    Direction dir_;
};

} // namespace model