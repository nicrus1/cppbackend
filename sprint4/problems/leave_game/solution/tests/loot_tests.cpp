#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../src/model.h"
#include "../src/loot_manager.h"
#include <chrono>

using namespace std::literals;

TEST_CASE("LootManager generates loot on roads") {
    model::Map map(model::Map::Id{"test"}, "Test Map");
    
    map.AddRoad(model::Road(model::Road::HORIZONTAL, model::Point{0, 0}, 10));
    map.AddRoad(model::Road(model::Road::VERTICAL, model::Point{0, 0}, 10));
    map.SetLootTypesCount(3);
    
    game::LootManager manager(map, 1.0, 1.0);
    
    manager.Update(1000ms, 5);
    
    auto loot = manager.GetLootItems();
    REQUIRE(loot.size() > 0);
    
    for (const auto& [id, item] : loot) {
        bool on_road = false;
        for (const auto& road : map.GetRoads()) {
            if (road.IsHorizontal()) {
                if (item.second.y == 0 && item.second.x >= 0 && item.second.x <= 10) {
                    on_road = true;
                    break;
                }
            } else {
                if (item.second.x == 0 && item.second.y >= 0 && item.second.y <= 10) {
                    on_road = true;
                    break;
                }
            }
        }
        REQUIRE(on_road);
    }
}

TEST_CASE("LootManager respects loot types count") {
    model::Map map(model::Map::Id{"test"}, "Test Map");
    map.AddRoad(model::Road(model::Road::HORIZONTAL, model::Point{0, 0}, 10));
    map.SetLootTypesCount(5);
    
    game::LootManager manager(map, 1.0, 1.0);
    manager.Update(1000ms, 10);
    
    auto loot = manager.GetLootItems();
    for (const auto& [id, item] : loot) {
        REQUIRE(item.first >= 0);
        REQUIRE(item.first < 5);
    }
}

TEST_CASE("LootManager loot count does not exceed dog count") {
    model::Map map(model::Map::Id{"test"}, "Test Map");
    map.AddRoad(model::Road(model::Road::HORIZONTAL, model::Point{0, 0}, 10));
    map.SetLootTypesCount(3);
    
    game::LootManager manager(map, 1.0, 1.0);
    
    manager.Update(1000ms, 10);
    auto loot = manager.GetLootItems();
    REQUIRE(loot.size() <= 10);
    
    manager.Update(1000ms, 5);
    loot = manager.GetLootItems();
    REQUIRE(loot.size() <= 15); // 10 + 5 = 15
}

TEST_CASE("LootManager generates random positions on different roads") {
    model::Map map(model::Map::Id{"test"}, "Test Map");
    map.AddRoad(model::Road(model::Road::HORIZONTAL, model::Point{0, 0}, 20));
    map.AddRoad(model::Road(model::Road::VERTICAL, model::Point{10, 0}, 20));
    map.AddRoad(model::Road(model::Road::HORIZONTAL, model::Point{0, 20}, 20));
    map.SetLootTypesCount(2);
    
    game::LootManager manager(map, 1.0, 1.0);
    manager.Update(1000ms, 20);
    
    auto loot = manager.GetLootItems();
    REQUIRE(loot.size() > 0);
    
    for (const auto& [id, item] : loot) {
        bool on_road = false;
        for (const auto& road : map.GetRoads()) {
            if (road.IsHorizontal()) {
                double y = static_cast<double>(road.GetStart().y);
                double x_min = std::min(road.GetStart().x, road.GetEnd().x);
                double x_max = std::max(road.GetStart().x, road.GetEnd().x);
                if (std::abs(item.second.y - y) < 0.001 && 
                    item.second.x >= x_min - 0.001 && 
                    item.second.x <= x_max + 0.001) {
                    on_road = true;
                    break;
                }
            } else {
                double x = static_cast<double>(road.GetStart().x);
                double y_min = std::min(road.GetStart().y, road.GetEnd().y);
                double y_max = std::max(road.GetStart().y, road.GetEnd().y);
                if (std::abs(item.second.x - x) < 0.001 && 
                    item.second.y >= y_min - 0.001 && 
                    item.second.y <= y_max + 0.001) {
                    on_road = true;
                    break;
                }
            }
        }
        REQUIRE(on_road);
    }
}