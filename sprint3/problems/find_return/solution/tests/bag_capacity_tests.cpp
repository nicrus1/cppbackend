#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../src/model.h"
#include "../src/game_state.h"
#include <chrono>

using namespace std::literals;

TEST_CASE("Dog bag capacity - default value") {
    model::Dog dog(1, {0.0, 0.0}, 1.0);
    
    // По умолчанию вместимость 3
    REQUIRE(dog.GetBagCapacity() == 3);
    REQUIRE(dog.IsBagFull() == false);
    REQUIRE(dog.GetBagSize() == 0);
}

TEST_CASE("Dog bag capacity - setting capacity") {
    model::Dog dog(1, {0.0, 0.0}, 1.0);
    dog.SetBagCapacity(5);
    
    REQUIRE(dog.GetBagCapacity() == 5);
    REQUIRE(dog.IsBagFull() == false);
}

TEST_CASE("Dog bag - adding items") {
    model::Dog dog(1, {0.0, 0.0}, 1.0);
    dog.SetBagCapacity(2);
    
    dog.AddToBag(1, 0);
    REQUIRE(dog.GetBagSize() == 1);
    REQUIRE(dog.IsBagFull() == false);
    
    dog.AddToBag(2, 1);
    REQUIRE(dog.GetBagSize() == 2);
    REQUIRE(dog.IsBagFull() == true);
    
    // Попытка добавить в полный рюкзак
    dog.AddToBag(3, 2);
    REQUIRE(dog.GetBagSize() == 2); // Не добавился
}

TEST_CASE("Dog bag - clearing bag") {
    model::Dog dog(1, {0.0, 0.0}, 1.0);
    dog.SetBagCapacity(5);
    
    dog.AddToBag(1, 0);
    dog.AddToBag(2, 1);
    dog.AddToBag(3, 2);
    
    REQUIRE(dog.GetBagSize() == 3);
    
    dog.ClearBag();
    REQUIRE(dog.GetBagSize() == 0);
    REQUIRE(dog.IsBagFull() == false);
}

TEST_CASE("Dog bag - bag items content") {
    model::Dog dog(1, {0.0, 0.0}, 1.0);
    dog.SetBagCapacity(3);
    
    dog.AddToBag(1, 0);
    dog.AddToBag(2, 1);
    dog.AddToBag(3, 2);
    
    const auto& bag = dog.GetBag();
    REQUIRE(bag.size() == 3);
    REQUIRE(bag[0].id == 1);
    REQUIRE(bag[0].type == 0);
    REQUIRE(bag[1].id == 2);
    REQUIRE(bag[1].type == 1);
    REQUIRE(bag[2].id == 3);
    REQUIRE(bag[2].type == 2);
}

TEST_CASE("Map bag capacity - default value") {
    model::Map map(model::Map::Id{"test"}, "Test Map");
    
    // По умолчанию 3
    REQUIRE(map.GetBagCapacity() == 3);
}

TEST_CASE("Map bag capacity - setting default") {
    model::Map map(model::Map::Id{"test"}, "Test Map");
    map.SetDefaultBagCapacity(5);
    
    REQUIRE(map.GetBagCapacity() == 5);
}

TEST_CASE("Map bag capacity - overriding for specific map") {
    model::Map map(model::Map::Id{"test"}, "Test Map");
    map.SetDefaultBagCapacity(3);
    map.SetBagCapacity(7);
    
    REQUIRE(map.GetBagCapacity() == 7);
}

TEST_CASE("GameState - dog gets bag capacity from map on join") {
    model::Map map(model::Map::Id{"test"}, "Test Map");
    map.SetDefaultDogSpeed(1.0);
    map.SetBagCapacity(4);
    map.AddRoad(model::Road(model::Road::HORIZONTAL, model::Point{0, 0}, 10));
    
    model::Game game;
    game.AddMap(std::move(map));
    
    game::GameState state(game);
    
    auto result = state.JoinGame("Player1", model::Map::Id{"test"});
    
    const auto* dog = state.GetDogByToken(result.token);
    REQUIRE(dog != nullptr);
    REQUIRE(dog->GetBagCapacity() == 4);
}

TEST_CASE("GameState - different maps have different bag capacities") {
    model::Map map1(model::Map::Id{"map1"}, "Map 1");
    map1.SetDefaultDogSpeed(1.0);
    map1.SetBagCapacity(3);
    map1.AddRoad(model::Road(model::Road::HORIZONTAL, model::Point{0, 0}, 10));
    
    model::Map map2(model::Map::Id{"map2"}, "Map 2");
    map2.SetDefaultDogSpeed(1.0);
    map2.SetBagCapacity(5);
    map2.AddRoad(model::Road(model::Road::HORIZONTAL, model::Point{0, 0}, 10));
    
    model::Game game;
    game.AddMap(std::move(map1));
    game.AddMap(std::move(map2));
    
    game::GameState state(game);
    
    auto result1 = state.JoinGame("Player1", model::Map::Id{"map1"});
    auto result2 = state.JoinGame("Player2", model::Map::Id{"map2"});
    
    const auto* dog1 = state.GetDogByToken(result1.token);
    const auto* dog2 = state.GetDogByToken(result2.token);
    
    REQUIRE(dog1 != nullptr);
    REQUIRE(dog2 != nullptr);
    REQUIRE(dog1->GetBagCapacity() == 3);
    REQUIRE(dog2->GetBagCapacity() == 5);
}