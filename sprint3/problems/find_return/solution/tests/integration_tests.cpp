#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../src/json_loader.h"
#include "../src/request_handler.h"
#include "../src/game_state.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>

using namespace std::literals;

TEST_CASE("Integration - full game flow with bag") {
    // Создаем тестовый конфигурационный файл
    std::string config_json = R"({
        "defaultDogSpeed": 3.0,
        "defaultBagCapacity": 3,
        "lootGeneratorConfig": {
            "period": 5.0,
            "probability": 0.5
        },
        "maps": [
            {
                "id": "test_map",
                "name": "Test Map",
                "bagCapacity": 4,
                "roads": [
                    {"x0": 0, "y0": 0, "x1": 20}
                ],
                "buildings": [],
                "offices": [
                    {"id": "o1", "x": 18, "y": 0, "offsetX": 2, "offsetY": 0}
                ],
                "lootTypes": [
                    {"name": "key", "file": "key.obj", "type": "obj", "rotation": 90, "color": "#338844", "scale": 0.03, "value": 10}
                ]
            }
        ]
    })";
    
    std::filesystem::path config_path = std::filesystem::temp_directory_path() / "test_config.json";
    std::ofstream file(config_path);
    file << config_json;
    file.close();
    
    model::Game game = json_loader::LoadGame(config_path);
    
    const auto* map = game.FindMap(model::Map::Id{"test_map"});
    REQUIRE(map != nullptr);
    REQUIRE(map->GetBagCapacity() == 4);
    
    std::filesystem::remove(config_path);
}

TEST_CASE("Integration - request handler loads bag capacity") {
    // Создаем тестовый конфигурационный файл
    std::string config_json = R"({
        "defaultDogSpeed": 3.0,
        "defaultBagCapacity": 3,
        "lootGeneratorConfig": {
            "period": 5.0,
            "probability": 0.5
        },
        "maps": [
            {
                "id": "test_map",
                "name": "Test Map",
                "bagCapacity": 4,
                "roads": [
                    {"x0": 0, "y0": 0, "x1": 20}
                ],
                "buildings": [],
                "offices": []
            }
        ]
    })";
    
    std::filesystem::path config_path = std::filesystem::temp_directory_path() / "test_config.json";
    std::ofstream file(config_path);
    file << config_json;
    file.close();
    
    model::Game game = json_loader::LoadGame(config_path);
    
    // Создаем RequestHandler
    http_handler::RequestHandler handler(game, "./static", true);
    handler.LoadExtraData(config_path);
    
    const auto* map = game.FindMap(model::Map::Id{"test_map"});
    REQUIRE(map != nullptr);
    REQUIRE(map->GetBagCapacity() == 4);
    
    std::filesystem::remove(config_path);
}

TEST_CASE("Integration - GameState processes ticks with bag") {
    model::Map map(model::Map::Id{"test_map"}, "Test Map");
    map.SetDefaultDogSpeed(1.0);
    map.SetBagCapacity(3);
    map.AddRoad(model::Road(model::Road::HORIZONTAL, model::Point{0, 0}, 20));
    map.SetLootTypesCount(2);
    
    model::Game game;
    game.AddMap(std::move(map));
    
    game::GameState state(game);
    state.SetLootGeneratorConfig(1.0, 1.0);
    
    auto result = state.JoinGame("Player1", model::Map::Id{"test_map"});
    auto token = result.token;
    
    state.SetDogDirection(token, model::Direction::EAST);
    
    // Двигаемся и собираем предметы
    state.ProcessTick(1000);
    state.ProcessTick(1000);
    state.ProcessTick(1000);
    
    auto states = state.GetGameState(token);
    REQUIRE(states.size() == 1);
    
    // Проверяем, что рюкзак не превышает вместимость
    REQUIRE(states[0].bag.size() <= 3);
}

TEST_CASE("Integration - game state response includes bag") {
    model::Map map(model::Map::Id{"test_map"}, "Test Map");
    map.SetDefaultDogSpeed(1.0);
    map.SetBagCapacity(3);
    map.AddRoad(model::Road(model::Road::HORIZONTAL, model::Point{0, 0}, 20));
    map.SetLootTypesCount(2);
    
    model::Game game;
    game.AddMap(std::move(map));
    
    game::GameState state(game);
    state.SetLootGeneratorConfig(1.0, 1.0);
    
    auto result = state.JoinGame("Player1", model::Map::Id{"test_map"});
    auto token = result.token;
    
    state.SetDogDirection(token, model::Direction::EAST);
    
    // Добавляем предметы в рюкзак напрямую для теста
    auto* dog = state.GetDogByTokenMutable(token);
    REQUIRE(dog != nullptr);
    dog->AddToBag(1, 0);
    dog->AddToBag(2, 1);
    
    auto states = state.GetGameState(token);
    REQUIRE(states.size() == 1);
    REQUIRE(states[0].bag.size() == 2);
    REQUIRE(states[0].bag[0].id == 1);
    REQUIRE(states[0].bag[0].type == 0);
    REQUIRE(states[0].bag[1].id == 2);
    REQUIRE(states[0].bag[1].type == 1);
}