#include <catch2/catch_test_macros.hpp>
#include <chrono>
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
                // With probability 1.0, shortage = looter_count - loot_count
                session.AddLooter();  // 1 looter
                session.Update(1s);
                CHECK(session.GetLostObjectsCount() == 1);
                
                session.Update(1s);
                CHECK(session.GetLostObjectsCount() == 1);  // Still 1 (loot == looter)
                
                session.AddLooter();  // 2 looters
                session.Update(1s);
                CHECK(session.GetLostObjectsCount() == 2);
            }
        }
        
        WHEN("generating lost objects") {
            session.AddLooter();
            session.Update(1s);
            
            THEN("lost objects should have valid types and positions") {
                auto& objects = session.GetLostObjects();
                REQUIRE(objects.size() == 1);
                
                auto& obj = objects.begin()->second;
                CHECK(obj.type < 3);
                CHECK(obj.position.x >= 0);
                CHECK(obj.position.x <= 10);
                CHECK(obj.position.y == 0);
            }
        }
        
        WHEN("multiple looters generate multiple objects") {
            for (int i = 0; i < 5; ++i) {
                session.AddLooter();
            }
            session.Update(1s);
            
            THEN("all generated objects should have unique IDs") {
                auto& objects = session.GetLostObjects();
                CHECK(objects.size() == 5);
                
                std::set<size_t> ids;
                for (const auto& [id, obj] : objects) {
                    ids.insert(*id);
                }
                CHECK(ids.size() == 5);
            }
        }
    }
}

SCENARIO("Loot generation on different roads") {
    GIVEN("a map with multiple roads") {
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
            bool horizontal_used = false;
            bool vertical_used = false;
            
            for (int i = 0; i < GENERATION_COUNT; ++i) {
                session.Update(1s);
                session.Update(0s);  // Force generation
            }
            
            auto& objects = session.GetLostObjects();
            CHECK(objects.size() > 0);
            
            // Check that we used both road types
            for (const auto& [id, obj] : objects) {
                if (obj.position.y == 0) {
                    horizontal_used = true;
                }
                if (obj.position.x == 5 && obj.position.y >= 0 && obj.position.y <= 10) {
                    vertical_used = true;
                }
            }
            
            CHECK(horizontal_used);
            CHECK(vertical_used);
        }
    }
}