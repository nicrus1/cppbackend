#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <set>
#include "../src/model.h"
#include "../src/loot_generator.h"

using namespace std::literals;

SCENARIO("Game session loot generation") {
    GIVEN("a game session with a map having 3 loot types") {
        auto map = std::make_shared<model::Map>(
            model::Map::Id("test_map"), "Test Map", 3
        );
        
        // Add a road
        model::Road road{{0, 0}, {10, 0}};
        map->AddRoad(road);
        
        model::GameSession session(
            model::GameSession::Id(0), 
            map,
            1s,
            1.0  // 100% probability for testing
        );
        
        WHEN("no looters and no loot") {
            THEN("generator should produce loot up to looter count") {
                session.AddLooter();  // 1 looter
                session.Update(1s);
                CHECK(session.GetLostObjectsCount() == 1);
                
                session.Update(1s);
                CHECK(session.GetLostObjectsCount() == 1);  // Still 1 (loot == looter)
                
                session.AddLooter();  // 2 looters
                // Увеличиваем время ожидания до 5 секунд, чтобы гарантировать генерацию
                session.Update(5s); 
                
                CHECK(session.GetLostObjectsCount() == 2);
            }
        }
    }
}

SCENARIO("Game session with multiple roads") {
    GIVEN("a game session with a map having multiple roads") {
        auto map = std::make_shared<model::Map>(
            model::Map::Id("multi_road_map"), "Multi Road Map", 2
        );
        
        // Horizontal road from (0,0) to (10,0)
        map->AddRoad({{0, 0}, {10, 0}});
        // Vertical road from (5,0) to (5,10)
        map->AddRoad({{5, 0}, {5, 10}});
        
        model::GameSession session(
            model::GameSession::Id(0),
            map,
            1s,
            1.0
        );
        
        session.AddLooter();
        
        WHEN("generating many objects") {
            const int GENERATION_COUNT = 100;
            
            for (int i = 0; i < GENERATION_COUNT; ++i) {
                session.Update(1s);
            }
            
            auto& objects = session.GetLostObjects();
            CHECK(objects.size() > 0);
            
            std::set<size_t> ids;
            for (const auto& [id, obj] : objects) {
                ids.insert(id.GetUnderlying());
            }
            
            CHECK(ids.size() == objects.size());
        }
    }
}