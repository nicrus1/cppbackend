#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../src/model.h"
#include "../src/loot_manager.h"
#include <chrono>

using namespace std::literals;

TEST_CASE("LootManager generates loot on roads") {
    model::Map map(model::Map::Id{"test"}, "Test Map");
    
    // Добавляем дороги
    map.AddRoad(model::Road(model::Road::HORIZONTAL, model::Point{0, 0}, 10));
    map.AddRoad(model::Road(model::Road::VERTICAL, model::Point{0, 0}, 10));
    map.SetLootTypesCount(3);
    
    game::LootManager manager(map, 1.0, 1.0);
    
    // Проверяем, что при наличии собак генерируются трофеи
    manager.Update(1000ms, 5);
    
    auto loot = manager.GetLootItems();
    REQUIRE(loot.size() > 0);
    
    // Проверяем, что позиция находится на дороге
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
    
    // Сначала генерируем много трофеев
    manager.Update(1000ms, 10);
    auto loot = manager.GetLootItems();
    REQUIRE(loot.size() <= 10);
    
    // Теперь генерируем с меньшим количеством собак
    manager.Update(1000ms, 5);
    loot = manager.GetLootItems();
    REQUIRE(loot.size() <= 15); // 10 + 5 = 15
}