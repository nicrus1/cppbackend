#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include "../src/model.h"

using namespace std::literals;

SCENARIO("Game session loot generation") {
    GIVEN("a game session with a map having 3 loot types") {
        auto map = std::make_shared<model::Map>(model::Map::Id("test_map"), "Test", 3);
        map->AddRoad({{0, 0}, {10, 0}});
        
        model::GameSession session(model::GameSession::Id(0), map, 1s, 1.0);
        
        WHEN("no looters and no loot") {
            THEN("generator should produce loot after update") {
                session.AddLooter();
                session.Update(2s); // Увеличен интервал для надежности
                CHECK(session.GetLostObjectsCount() == 1);
            }
        }
    }
}