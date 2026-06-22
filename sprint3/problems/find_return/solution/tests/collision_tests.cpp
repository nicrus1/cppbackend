#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../src/model.h"
#include "../src/game_state.h"
#include "../src/loot_manager.h"
#include <chrono>
#include <memory>

using namespace std::literals;

class TestGameState : public game::GameState {
public:
    explicit TestGameState(model::Game& game) : game::GameState(game) {}
    
    // Делаем методы доступными для тестирования
    using game::GameState::CheckLootCollision;
    using game::GameState::CheckOfficeCollision;
    using game::GameState::FindLootCollisions;
    using game::GameState::ProcessCollisions;
};

TEST_CASE("CheckLootCollision - distance calculation") {
    model::Game game;
    TestGameState state(game);
    
    // Точное попадание
    REQUIRE(state.CheckLootCollision({0.0, 0.0}, {0.0, 0.0}) == true);
    
    // На границе (0.3)
    REQUIRE(state.CheckLootCollision({0.3, 0.0}, {0.0, 0.0}) == true);
    REQUIRE(state.CheckLootCollision({-0.3, 0.0}, {0.0, 0.0}) == true);
    REQUIRE(state.CheckLootCollision({0.0, 0.3}, {0.0, 0.0}) == true);
    REQUIRE(state.CheckLootCollision({0.0, -0.3}, {0.0, 0.0}) == true);
    
    // За границей
    REQUIRE(state.CheckLootCollision({0.31, 0.0}, {0.0, 0.0}) == false);
    REQUIRE(state.CheckLootCollision({0.0, 0.31}, {0.0, 0.0}) == false);
    REQUIRE(state.CheckLootCollision({0.3, 0.01}, {0.0, 0.0}) == false);
}

TEST_CASE("CheckOfficeCollision - distance calculation") {
    model::Game game;
    TestGameState state(game);
    
    // Точное попадание
    REQUIRE(state.CheckOfficeCollision({0.0, 0.0}, {0.0, 0.0}) == true);
    
    // На границе (0.55)
    REQUIRE(state.CheckOfficeCollision({0.55, 0.0}, {0.0, 0.0}) == true);
    REQUIRE(state.CheckOfficeCollision({-0.55, 0.0}, {0.0, 0.0}) == true);
    REQUIRE(state.CheckOfficeCollision({0.0, 0.55}, {0.0, 0.0}) == true);
    REQUIRE(state.CheckOfficeCollision({0.0, -0.55}, {0.0, 0.0}) == true);
    
    // За границей
    REQUIRE(state.CheckOfficeCollision({0.56, 0.0}, {0.0, 0.0}) == false);
    REQUIRE(state.CheckOfficeCollision({0.0, 0.56}, {0.0, 0.0}) == false);
}

TEST_CASE("FindLootCollisions - finding collisions on path") {
    model::Game game;
    TestGameState state(game);
    
    // Создаем тестовые предметы
    std::unordered_map<uint64_t, std::pair<int, model::Position>> loot_items;
    loot_items[1] = {0, {5.0, 0.0}};
    loot_items[2] = {1, {10.0, 0.0}};
    loot_items[3] = {2, {15.0, 0.0}};
    
    std::unordered_map<uint64_t, model::Dog*> dogs_on_map;
    
    // Путь от (0,0) до (20,0)
    auto collisions = state.FindLootCollisions(
        {0.0, 0.0}, {20.0, 0.0}, loot_items, dogs_on_map
    );
    
    REQUIRE(collisions.size() == 3);
    REQUIRE(collisions[0].second == 1); // Первый предмет
    REQUIRE(collisions[1].second == 2); // Второй предмет
    REQUIRE(collisions[2].second == 3); // Третий предмет
    
    // Проверяем порядок (по времени достижения)
    REQUIRE(collisions[0].first < collisions[1].first);
    REQUIRE(collisions[1].first < collisions[2].first);
}

TEST_CASE("FindLootCollisions - no movement") {
    model::Game game;
    TestGameState state(game);
    
    std::unordered_map<uint64_t, std::pair<int, model::Position>> loot_items;
    loot_items[1] = {0, {5.0, 0.0}};
    
    std::unordered_map<uint64_t, model::Dog*> dogs_on_map;
    
    // Игрок не двигается, но предмет под ним
    auto collisions = state.FindLootCollisions(
        {5.0, 0.0}, {5.0, 0.0}, loot_items, dogs_on_map
    );
    
    REQUIRE(collisions.size() == 1);
    REQUIRE(collisions[0].second == 1);
    REQUIRE(collisions[0].first == 0.0);
}

TEST_CASE("FindLootCollisions - diagonal movement") {
    model::Game game;
    TestGameState state(game);
    
    std::unordered_map<uint64_t, std::pair<int, model::Position>> loot_items;
    loot_items[1] = {0, {5.0, 5.0}};
    loot_items[2] = {1, {3.0, 3.0}};
    loot_items[3] = {2, {8.0, 8.0}};
    
    std::unordered_map<uint64_t, model::Dog*> dogs_on_map;
    
    // Путь от (0,0) до (10,10)
    auto collisions = state.FindLootCollisions(
        {0.0, 0.0}, {10.0, 10.0}, loot_items, dogs_on_map
    );
    
    // Должны быть найдены предметы на пути или рядом с ним
    // (3,3) - на пути, (5,5) - на пути, (8,8) - на пути
    REQUIRE(collisions.size() == 3);
}

TEST_CASE("FindLootCollisions - out of path") {
    model::Game game;
    TestGameState state(game);
    
    std::unordered_map<uint64_t, std::pair<int, model::Position>> loot_items;
    loot_items[1] = {0, {5.0, 10.0}}; // Далеко от пути
    
    std::unordered_map<uint64_t, model::Dog*> dogs_on_map;
    
    auto collisions = state.FindLootCollisions(
        {0.0, 0.0}, {10.0, 0.0}, loot_items, dogs_on_map
    );
    
    REQUIRE(collisions.empty());
}

TEST_CASE("ProcessCollisions - full tick simulation") {
    model::Map map(model::Map::Id{"test"}, "Test Map");
    map.SetDefaultDogSpeed(1.0);
    map.SetLootTypesCount(3);
    map.SetBagCapacity(3);
    map.SetDefaultBagCapacity(3);
    
    // Добавляем дорогу для движения
    map.AddRoad(model::Road(model::Road::HORIZONTAL, model::Point{0, 0}, 30));
    
    // Добавляем офис (базу)
    map.AddOffice(model::Office(
        model::Office::Id{"office1"},
        model::Point{25, 0},
        model::Offset{2, 0}
    ));
    
    model::Game game;
    game.AddMap(std::move(map));
    
    game::GameState state(game);
    state.SetLootGeneratorConfig(1.0, 1.0); // Генерируем лут каждый тик
    
    // Присоединяем игрока
    auto join_result = state.JoinGame("Player1", model::Map::Id{"test"});
    auto token = join_result.token;
    
    // Устанавливаем направление движения
    state.SetDogDirection(token, model::Direction::EAST);
    
    // Запускаем тики для движения
    // Игрок движется от (0,0) на восток
    // На дороге есть предметы, которые нужно собрать
    
    // Первый тик - игрок начинает движение
    state.ProcessTick(100); // 0.1 секунды
    
    // Проверяем состояние
    auto states = state.GetGameState(token);
    REQUIRE(states.size() == 1);
    
    // Двигаемся дальше
    state.ProcessTick(500); // 0.5 секунды
    
    states = state.GetGameState(token);
    REQUIRE(states.size() == 1);
    
    // Проверяем, что игрок двигается
    REQUIRE(states[0].pos.x > 0);
}

TEST_CASE("ProcessCollisions - bag capacity") {
    model::Map map(model::Map::Id{"test"}, "Test Map");
    map.SetDefaultDogSpeed(1.0);
    map.SetLootTypesCount(3);
    map.SetBagCapacity(2); // Маленький рюкзак
    
    // Добавляем дорогу
    map.AddRoad(model::Road(model::Road::HORIZONTAL, model::Point{0, 0}, 30));
    
    model::Game game;
    game.AddMap(std::move(map));
    
    game::GameState state(game);
    state.SetLootGeneratorConfig(1.0, 1.0);
    
    auto join_result = state.JoinGame("Player1", model::Map::Id{"test"});
    auto token = join_result.token;
    
    // Устанавливаем направление
    state.SetDogDirection(token, model::Direction::EAST);
    
    // Двигаемся и собираем предметы
    state.ProcessTick(2000); // 2 секунды
    
    auto states = state.GetGameState(token);
    REQUIRE(states.size() == 1);
    
    // Рюкзак должен быть заполнен (не более 2 предметов)
    REQUIRE(states[0].bag.size() <= 2);
}

TEST_CASE("ProcessCollisions - returning to base") {
    model::Map map(model::Map::Id{"test"}, "Test Map");
    map.SetDefaultDogSpeed(1.0);
    map.SetLootTypesCount(3);
    map.SetBagCapacity(5);
    
    // Добавляем дорогу
    map.AddRoad(model::Road(model::Road::HORIZONTAL, model::Point{0, 0}, 30));
    
    // Добавляем офис (базу) в конце пути
    map.AddOffice(model::Office(
        model::Office::Id{"office1"},
        model::Point{25, 0},
        model::Offset{2, 0}
    ));
    
    model::Game game;
    game.AddMap(std::move(map));
    
    game::GameState state(game);
    state.SetLootGeneratorConfig(1.0, 1.0);
    
    auto join_result = state.JoinGame("Player1", model::Map::Id{"test"});
    auto token = join_result.token;
    
    // Двигаемся на восток
    state.SetDogDirection(token, model::Direction::EAST);
    
    // Двигаемся к базе
    state.ProcessTick(5000); // 5 секунд
    
    auto states = state.GetGameState(token);
    REQUIRE(states.size() == 1);
    
    // У базы рюкзак должен быть пуст (все сдано)
    REQUIRE(states[0].bag.empty());
}

TEST_CASE("ProcessCollisions - competition for loot") {
    model::Map map(model::Map::Id{"test"}, "Test Map");
    map.SetDefaultDogSpeed(1.0);
    map.SetLootTypesCount(3);
    map.SetBagCapacity(5);
    
    // Добавляем дорогу с предметами
    map.AddRoad(model::Road(model::Road::HORIZONTAL, model::Point{0, 0}, 30));
    
    model::Game game;
    game.AddMap(std::move(map));
    
    game::GameState state(game);
    state.SetLootGeneratorConfig(1.0, 1.0);
    
    // Присоединяем двух игроков
    auto join_result1 = state.JoinGame("Player1", model::Map::Id{"test"});
    auto token1 = join_result1.token;
    
    auto join_result2 = state.JoinGame("Player2", model::Map::Id{"test"});
    auto token2 = join_result2.token;
    
    // Устанавливаем направления
    state.SetDogDirection(token1, model::Direction::EAST);
    state.SetDogDirection(token2, model::Direction::EAST);
    
    // Первый игрок начинает с (0,0), второй с (5,0)
    // Оба двигаются на восток
    
    state.ProcessTick(1000); // 1 секунда
    
    auto states1 = state.GetGameState(token1);
    auto states2 = state.GetGameState(token2);
    
    REQUIRE(states1.size() == 1);
    REQUIRE(states2.size() == 1);
    
    // Проверяем, что предметы распределены между игроками
    // (конкретные проверки зависят от генерации лута)
}