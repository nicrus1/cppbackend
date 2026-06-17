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
        
        // Передаем детерминированную лямбду []{ return 1.0; }, чтобы тест не зависел от случайности
        model::GameSession session(
            model::GameSession::Id(0), 
            map,
            1s,
            1.0,
            []() { return 1.0; }
        );
        
        WHEN("no looters and no loot") {
            THEN("generator should produce loot up to looter count") {
                session.AddLooter();  // 1 looter
                
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
        
        // Здесь также фиксируем генератор случайных чисел для стабильности
        model::GameSession session(
            model::GameSession::Id(0),
            map,
            1s,
            1.0,
            []() { return 1.0; }
        );
        
        session.AddLooter();
        
        WHEN("generating many objects") {
            for (int i = 0; i < 5; ++i) {
                session.Update(2s);
            }
            
            auto& objects = session.GetLostObjects();
            CHECK(objects.size() > 0);
            
            std::set<size_t> types;
            for (const auto& [id, obj] : objects) {
                types.insert(obj.type);
            }
            
            for (auto type : types) {
                CHECK(type < map->GetLootTypesCount());
            }
        }
    }
}