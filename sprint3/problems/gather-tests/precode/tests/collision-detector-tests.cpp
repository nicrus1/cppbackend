#define _USE_MATH_DEFINES

#include "../src/collision_detector.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <sstream>
#include <cmath>

// Подсказка из задания: специализация StringMaker для печати событий в Catch2
namespace Catch {
template<>
struct StringMaker<collision_detector::GatheringEvent> {
    static std::string convert(collision_detector::GatheringEvent const& value) {
        std::ostringstream tmp;
        tmp << "(" << value.gatherer_id << "," << value.item_id << "," 
            << value.sq_distance << "," << value.time << ")";
        return tmp.str();
    }
};
}  // namespace Catch

namespace collision_detector {

// Оператор сравнения с допустимой погрешностью (требование 10⁻¹⁰)
bool operator==(const GatheringEvent& l, const GatheringEvent& r) {
    const double EPSILON = 1e-10;
    return l.gatherer_id == r.gatherer_id &&
           l.item_id == r.item_id &&
           std::abs(l.sq_distance - r.sq_distance) < EPSILON &&
           std::abs(l.time - r.time) < EPSILON;
}

} // namespace collision_detector

// Создаем тестового провайдера
class TestProvider : public collision_detector::ItemGathererProvider {
public:
    std::vector<collision_detector::Item> items_;
    std::vector<collision_detector::Gatherer> gatherers_;

    size_t ItemsCount() const override { return items_.size(); }
    collision_detector::Item GetItem(size_t idx) const override { return items_[idx]; }
    size_t GatherersCount() const override { return gatherers_.size(); }
    collision_detector::Gatherer GetGatherer(size_t idx) const override { return gatherers_[idx]; }
};

TEST_CASE("FindGatherEvents detecting collisions", "[collision_detector]") {
    TestProvider provider;

    SECTION("No movement - no events") {
        provider.items_.push_back({ {5.0, 0.0}, 1.0 });
        provider.gatherers_.push_back({ {0.0, 0.0}, {0.0, 0.0}, 1.0 }); // Не двигается
        
        auto events = collision_detector::FindGatherEvents(provider);
        REQUIRE(events.empty());
    }

    SECTION("Gatherer collects an item") {
        provider.items_.push_back({ {5.0, 0.0}, 1.0 });
        provider.gatherers_.push_back({ {0.0, 0.0}, {10.0, 0.0}, 1.0 });

        auto events = collision_detector::FindGatherEvents(provider);
        
        std::vector<collision_detector::GatheringEvent> expected = {
            {0, 0, 0.0, 0.5} // item_id, gatherer_id, sq_distance, time
        };
        REQUIRE(events == expected);
    }

    SECTION("Gatherer misses an item (too far)") {
        provider.items_.push_back({ {5.0, 5.0}, 1.0 }); // w = 1.0
        provider.gatherers_.push_back({ {0.0, 0.0}, {10.0, 0.0}, 1.0 }); // W = 1.0
        // Расстояние 5.0, а радиус захвата w+W = 2.0. Коллизии нет.

        auto events = collision_detector::FindGatherEvents(provider);
        REQUIRE(events.empty());
    }

    SECTION("Multiple events are sorted chronologically") {
        provider.items_.push_back({ {8.0, 0.0}, 0.5 }); // Подберет вторым (time = 0.8)
        provider.items_.push_back({ {2.0, 0.0}, 0.5 }); // Подберет первым (time = 0.2)
        provider.gatherers_.push_back({ {0.0, 0.0}, {10.0, 0.0}, 0.5 });

        auto events = collision_detector::FindGatherEvents(provider);

        // Проверяем, что события отсортированы по time
        std::vector<collision_detector::GatheringEvent> expected = {
            {1, 0, 0.0, 0.2},
            {0, 0, 0.0, 0.8}
        };
        REQUIRE(events == expected);
    }
}