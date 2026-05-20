#pragma once
#include <string>

class Player {
public:
    Player(int id, std::string name, const std::string& mapId, int dogId)
        : id_(id), name_(std::move(name)), mapId_(mapId), dogId_(dogId) {}

    int getId() const { return id_; }
    const std::string& getName() const { return name_; }
    const std::string& getMapId() const { return mapId_; }
    int getDogId() const { return dogId_; }

private:
    int id_;
    std::string name_;
    std::string mapId_;
    int dogId_;
};