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
        
        map->AddRoad({{0, 0}, {10, 0}});
        
        model::GameSession session(
            model::GameSession::Id(0), 
            map,
            1s,
            1.0  // 100% probability for testing
        );
        
        WHEN("no looters and no loot") {
            THEN("generator should produce loot up to looter count") {
                session.AddLooter();
                session.Update(2s); // Увеличено время для гарантии генерации
                CHECK(session.GetLostObjectsCount() == 1);
                
                session.Update(2s);
                CHECK(session.GetLostObjectsCount() == 1);
                
                session.AddLooter();
                session.Update(2s);
                CHECK(session.GetLostObjectsCount() == 2);
            }
        }

        WHEN("multiple looters generate multiple objects") {
            for (int i = 0; i < 5; ++i) {
                session.AddLooter();
            }
            session.Update(2s);
            
            THEN("all generated objects should have unique IDs") {
                auto& objects = session.GetLostObjects();
                CHECK(objects.size() == 5);
                
                std::set<size_t> ids;
                for (const auto& [id, obj] : objects) {
                    ids.insert(id.GetUnderlying()); // Исправлено обращение к ID
                }
                CHECK(ids.size() == 5);
            }
        }
    }
}