#define _USE_MATH_DEFINES

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include "../src/collision_detector.h"
#include <sstream>
#include <vector>
#include <cmath>

namespace Catch {
template<>
struct StringMaker<collision_detector::GatheringEvent> {
  static std::string convert(collision_detector::GatheringEvent const& value) {
      std::ostringstream tmp;
      tmp << "(Gatherer: " << value.gatherer_id 
          << ", Item: " << value.item_id 
          << ", SqDist: " << value.sq_distance 
          << ", Time: " << value.time << ")";
      return tmp.str();
  }
};
}  // namespace Catch

using namespace collision_detector;

// Вспомогательный класс-провайдер для тестов
class TestProvider : public ItemGathererProvider {
public:
    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;

    size_t ItemsCount() const override { return items_.size(); }
    Item GetItem(size_t idx) const override { return items_[idx]; }
    size_t GatherersCount() const override { return gatherers_.size(); }
    Gatherer GetGatherer(size_t idx) const override { return gatherers_[idx]; }
};

// Компаратор для событий с учетом погрешности 10^-10
class CompareEventsMatcher : public Catch::Matchers::MatcherBase<std::vector<GatheringEvent>> {
    std::vector<GatheringEvent> expected_;
public:
    CompareEventsMatcher(std::vector<GatheringEvent> expected) : expected_(std::move(expected)) {}

    bool match(const std::vector<GatheringEvent>& result) const override {
        if (result.size() != expected_.size()) return false;
        for (size_t i = 0; i < result.size(); ++i) {
            if (result[i].item_id != expected_[i].item_id ||
                result[i].gatherer_id != expected_[i].gatherer_id ||
                std::abs(result[i].sq_distance - expected_[i].sq_distance) > 1e-10 ||
                std::abs(result[i].time - expected_[i].time) > 1e-10) {
                return false;
            }
        }
        return true;
    }

    std::string describe() const override {
        return "Matches expected GatheringEvents with precision 1e-10";
    }
};

CompareEventsMatcher IsEqualEvents(const std::vector<GatheringEvent>& expected) {
    return CompareEventsMatcher(expected);
}

TEST_CASE("FindGatherEvents correctly detects collisions", "[collision_detector]") {
    TestProvider provider;

    SECTION("No gatherers or items") {
        auto events = FindGatherEvents(provider);
        REQUIRE(events.empty());
    }

    SECTION("Gatherer picks up an item on a straight line") {
        provider.gatherers_.push_back({geom::Point2D{0, 0}, geom::Point2D{10, 0}, 1.0});
        provider.items_.push_back({geom::Point2D{5, 0}, 0.5});

        auto events = FindGatherEvents(provider);
        
        std::vector<GatheringEvent> expected = {{0, 0, 0.0, 0.5}};
        REQUIRE_THAT(events, IsEqualEvents(expected));
    }

    SECTION("Gatherer picks up multiple items, chronological sorting") {
        provider.gatherers_.push_back({geom::Point2D{0, 0}, geom::Point2D{10, 0}, 0.5});
        // Расположим предметы в разном порядке, чтобы проверить сортировку
        provider.items_.push_back({geom::Point2D{8, 0.2}, 0.5}); // Время 0.8
        provider.items_.push_back({geom::Point2D{2, -0.2}, 0.5}); // Время 0.2
        provider.items_.push_back({geom::Point2D{5, 0.0}, 0.5}); // Время 0.5

        auto events = FindGatherEvents(provider);
        
        std::vector<GatheringEvent> expected = {
            {1, 0, 0.04, 0.2},
            {2, 0, 0.0, 0.5},
            {0, 0, 0.04, 0.8}
        };
        REQUIRE_THAT(events, IsEqualEvents(expected));
    }

    SECTION("Item is too far away (ignores missed items)") {
        provider.gatherers_.push_back({geom::Point2D{0, 0}, geom::Point2D{10, 0}, 0.5});
        provider.items_.push_back({geom::Point2D{5, 2}, 0.5}); // Расстояние 2 > 0.5 + 0.5

        auto events = FindGatherEvents(provider);
        REQUIRE(events.empty());
    }

    SECTION("Item is behind or ahead of gatherer trajectory") {
        provider.gatherers_.push_back({geom::Point2D{0, 0}, geom::Point2D{10, 0}, 1.0});
        provider.items_.push_back({geom::Point2D{-1, 0}, 0.5}); // Позади (time < 0)
        provider.items_.push_back({geom::Point2D{12, 0}, 0.5}); // Впереди (time > 1)

        auto events = FindGatherEvents(provider);
        REQUIRE(events.empty());
    }

    SECTION("Gatherer does not move") {
        provider.gatherers_.push_back({geom::Point2D{5, 0}, geom::Point2D{5, 0}, 1.0});
        provider.items_.push_back({geom::Point2D{5, 0}, 0.5}); // Идеальное совпадение, но нет перемещения

        auto events = FindGatherEvents(provider);
        REQUIRE(events.empty()); // Если объект не переместился, он не совершил столкновений
    }
}