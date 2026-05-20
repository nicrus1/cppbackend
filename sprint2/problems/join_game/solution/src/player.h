#pragma once
#include "tagged.h"
#include "model.h"
#include <string>
#include <cstdint>

namespace model {

struct PlayerTag {};
using PlayerId = util::Tagged<uint64_t, PlayerTag>;

class Player {
public:
    Player() = default;
    
    Player(PlayerId id, std::string name, const Map::Id& map_id)
        : id_(std::move(id))
        , name_(std::move(name))
        , map_id_(map_id) {}
    
    PlayerId GetId() const { return id_; }
    const std::string& GetName() const { return name_; }
    const Map::Id& GetMapId() const { return map_id_; }
    
    void SetDogId(uint64_t dog_id) { dog_id_ = dog_id; }
    uint64_t GetDogId() const { return dog_id_; }

private:
    PlayerId id_{0};
    std::string name_;
    Map::Id map_id_{""};
    uint64_t dog_id_{0};
};

}  // namespace model