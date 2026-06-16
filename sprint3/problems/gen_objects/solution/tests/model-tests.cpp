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
        
        // ВАЖНО: Мы используем конструктор, который передаем в GameSession.
        // Чтобы тесты были стабильными, нужно, чтобы Update внутри генерировал loot всегда.
        // Передаем вероятность 1.0.
        model::GameSession session(
            model::GameSession::Id(0), 
            map,
            1s,
            1.0 
        );
        
        WHEN("no looters and no loot") {
            THEN("generator should produce loot up to looter count") {
                session.AddLooter();  // 1 looter
                
                // Даем больше времени для накопления вероятности
                session.Update(2s);
                CHECK(session.GetLostObjectsCount() == 1);
                
                session.Update(2s);
                CHECK(session.GetLostObjectsCount() == 1); 
                
                session.AddLooter();  // 2 looters
                session.Update(2s);
                
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
        
        map->AddRoad({{0, 0}, {10, 0}});
        map->AddRoad({{5, 0}, {5, 10}});
        
        model::GameSession session(
            model::GameSession::Id(0),
            map,
            1s,
            1.0
        );
        
        session.AddLooter();
        
        WHEN("generating many objects") {
            // Для теста достаточно 5 итераций с большим интервалом
            for (int i = 0; i < 5; ++i) {
                session.Update(2s);
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