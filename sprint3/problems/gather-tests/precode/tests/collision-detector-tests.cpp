#define _USE_MATH_DEFINES

#include "../src/collision_detector.h"
#include <catch2/catch_all.hpp>
#include <cmath>
#include <vector>
#include <algorithm>
#include <sstream>

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

namespace {

const double EPS = 1e-10;

// Test provider implementation
class TestProvider : public collision_detector::ItemGathererProvider {
public:
    TestProvider() = default;
    
    TestProvider(std::vector<collision_detector::Item> items, 
                 std::vector<collision_detector::Gatherer> gatherers)
        : items_(std::move(items)), gatherers_(std::move(gatherers)) {}

    size_t ItemsCount() const override {
        return items_.size();
    }

    collision_detector::Item GetItem(size_t idx) const override {
        return items_[idx];
    }

    size_t GatherersCount() const override {
        return gatherers_.size();
    }

    collision_detector::Gatherer GetGatherer(size_t idx) const override {
        return gatherers_[idx];
    }

    void AddItem(collision_detector::Item item) {
        items_.push_back(item);
    }

    void AddGatherer(collision_detector::Gatherer gatherer) {
        gatherers_.push_back(gatherer);
    }

private:
    std::vector<collision_detector::Item> items_;
    std::vector<collision_detector::Gatherer> gatherers_;
};

bool IsEventEqual(const collision_detector::GatheringEvent& a, 
                  const collision_detector::GatheringEvent& b) {
    return a.item_id == b.item_id &&
           a.gatherer_id == b.gatherer_id &&
           std::abs(a.sq_distance - b.sq_distance) < EPS &&
           std::abs(a.time - b.time) < EPS;
}

bool AreEventsEqual(const std::vector<collision_detector::GatheringEvent>& a,
                    const std::vector<collision_detector::GatheringEvent>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (!IsEventEqual(a[i], b[i])) return false;
    }
    return true;
}

bool IsSortedByTime(const std::vector<collision_detector::GatheringEvent>& events) {
    for (size_t i = 1; i < events.size(); ++i) {
        if (events[i-1].time > events[i].time + EPS) return false;
    }
    return true;
}

}  // namespace

TEST_CASE("FindGatherEvents detects direct hit", "[collision_detector]") {
    // Item at (0, 0) with width 0.6
    // Gatherer moves from (-1, 0) to (1, 0) with width 0.6
    // Item is exactly on the path
    TestProvider provider;
    provider.AddItem({geom::Point2D{0.0, 0.0}, 0.6});
    provider.AddGatherer({geom::Point2D{-1.0, 0.0}, geom::Point2D{1.0, 0.0}, 0.6});

    auto events = collision_detector::FindGatherEvents(provider);
    
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].gatherer_id == 0);
    REQUIRE(events[0].item_id == 0);
    REQUIRE(std::abs(events[0].sq_distance - 0.0) < EPS);
    REQUIRE(std::abs(events[0].time - 0.5) < EPS);
}

TEST_CASE("FindGatherEvents detects hit with offset", "[collision_detector]") {
    // Item at (0, 0.3) with width 0.6 (radius 0.3)
    // Gatherer moves from (-1, 0) to (1, 0) with width 0.6 (radius 0.3)
    // Distance from path = 0.3, sum of radii = 0.6 -> should collect
    TestProvider provider;
    provider.AddItem({geom::Point2D{0.0, 0.3}, 0.6});
    provider.AddGatherer({geom::Point2D{-1.0, 0.0}, geom::Point2D{1.0, 0.0}, 0.6});

    auto events = collision_detector::FindGatherEvents(provider);
    
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].gatherer_id == 0);
    REQUIRE(events[0].item_id == 0);
    REQUIRE(std::abs(events[0].sq_distance - 0.09) < EPS);
    REQUIRE(std::abs(events[0].time - 0.5) < EPS);
}

TEST_CASE("FindGatherEvents detects no hit when too far", "[collision_detector]") {
    // Item at (0, 0.4) with width 0.6 (radius 0.3)
    // Gatherer moves from (-1, 0) to (1, 0) with width 0.6 (radius 0.3)
    // Distance from path = 0.4, sum of radii = 0.6 -> should NOT collect
    TestProvider provider;
    provider.AddItem({geom::Point2D{0.0, 0.4}, 0.6});
    provider.AddGatherer({geom::Point2D{-1.0, 0.0}, geom::Point2D{1.0, 0.0}, 0.6});

    auto events = collision_detector::FindGatherEvents(provider);
    
    REQUIRE(events.empty());
}

TEST_CASE("FindGatherEvents detects no hit before start", "[collision_detector]") {
    // Item behind start position
    TestProvider provider;
    provider.AddItem({geom::Point2D{-1.5, 0.0}, 0.6});
    provider.AddGatherer({geom::Point2D{-1.0, 0.0}, geom::Point2D{1.0, 0.0}, 0.6});

    auto events = collision_detector::FindGatherEvents(provider);
    REQUIRE(events.empty());
}

TEST_CASE("FindGatherEvents detects no hit after end", "[collision_detector]") {
    // Item beyond end position
    TestProvider provider;
    provider.AddItem({geom::Point2D{1.5, 0.0}, 0.6});
    provider.AddGatherer({geom::Point2D{-1.0, 0.0}, geom::Point2D{1.0, 0.0}, 0.6});

    auto events = collision_detector::FindGatherEvents(provider);
    REQUIRE(events.empty());
}

TEST_CASE("FindGatherEvents detects multiple items", "[collision_detector]") {
    // Two items on the path
    TestProvider provider;
    provider.AddItem({geom::Point2D{-0.5, 0.0}, 0.6});
    provider.AddItem({geom::Point2D{0.5, 0.0}, 0.6});
    provider.AddGatherer({geom::Point2D{-1.0, 0.0}, geom::Point2D{1.0, 0.0}, 0.6});

    auto events = collision_detector::FindGatherEvents(provider);
    
    REQUIRE(events.size() == 2);
    REQUIRE(events[0].gatherer_id == 0);
    REQUIRE(events[0].item_id == 0);
    REQUIRE(std::abs(events[0].time - 0.25) < EPS);
    REQUIRE(events[1].gatherer_id == 0);
    REQUIRE(events[1].item_id == 1);
    REQUIRE(std::abs(events[1].time - 0.75) < EPS);
}

TEST_CASE("FindGatherEvents detects multiple gatherers", "[collision_detector]") {
    // Two gatherers, two items
    TestProvider provider;
    provider.AddItem({geom::Point2D{0.0, 0.0}, 0.6});
    provider.AddItem({geom::Point2D{0.0, 0.5}, 0.6});
    provider.AddGatherer({geom::Point2D{-1.0, 0.0}, geom::Point2D{1.0, 0.0}, 0.6});
    provider.AddGatherer({geom::Point2D{-1.0, 0.5}, geom::Point2D{1.0, 0.5}, 0.6});

    auto events = collision_detector::FindGatherEvents(provider);
    
    REQUIRE(events.size() == 2);
    // First gatherer collects first item at 0.5
    // Second gatherer collects second item at 0.5
    bool found1 = false, found2 = false;
    for (const auto& e : events) {
        if (e.gatherer_id == 0 && e.item_id == 0) {
            found1 = true;
            REQUIRE(std::abs(e.time - 0.5) < EPS);
        }
        if (e.gatherer_id == 1 && e.item_id == 1) {
            found2 = true;
            REQUIRE(std::abs(e.time - 0.5) < EPS);
        }
    }
    REQUIRE(found1);
    REQUIRE(found2);
}

TEST_CASE("FindGatherEvents events are sorted by time", "[collision_detector]") {
    // Three items at different positions along the path
    TestProvider provider;
    provider.AddItem({geom::Point2D{-0.75, 0.0}, 0.6});
    provider.AddItem({geom::Point2D{0.0, 0.0}, 0.6});
    provider.AddItem({geom::Point2D{0.75, 0.0}, 0.6});
    provider.AddGatherer({geom::Point2D{-1.0, 0.0}, geom::Point2D{1.0, 0.0}, 0.6});

    auto events = collision_detector::FindGatherEvents(provider);
    
    REQUIRE(events.size() == 3);
    REQUIRE(IsSortedByTime(events));
    REQUIRE(std::abs(events[0].time - 0.125) < EPS);
    REQUIRE(std::abs(events[1].time - 0.5) < EPS);
    REQUIRE(std::abs(events[2].time - 0.875) < EPS);
}

TEST_CASE("FindGatherEvents respects item width", "[collision_detector]") {
    // Large item width 1.0 (radius 0.5) should be collected even if center is far
    TestProvider provider;
    provider.AddItem({geom::Point2D{0.0, 0.45}, 1.0});
    provider.AddGatherer({geom::Point2D{-1.0, 0.0}, geom::Point2D{1.0, 0.0}, 0.6});

    auto events = collision_detector::FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
}

TEST_CASE("FindGatherEvents respects gatherer width", "[collision_detector]") {
    // Wide gatherer width 1.0 (radius 0.5) should collect item that's far from path
    TestProvider provider;
    provider.AddItem({geom::Point2D{0.0, 0.45}, 0.6});
    provider.AddGatherer({geom::Point2D{-1.0, 0.0}, geom::Point2D{1.0, 0.0}, 1.0});

    auto events = collision_detector::FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
}

TEST_CASE("FindGatherEvents no zero movement", "[collision_detector]") {
    // Gatherer doesn't move - should detect no collisions
    TestProvider provider;
    provider.AddItem({geom::Point2D{0.0, 0.0}, 0.6});
    provider.AddGatherer({geom::Point2D{0.0, 0.0}, geom::Point2D{0.0, 0.0}, 0.6});

    auto events = collision_detector::FindGatherEvents(provider);
    REQUIRE(events.empty());
}

TEST_CASE("FindGatherEvents diagonal movement", "[collision_detector]") {
    // Diagonal movement from (-1, -1) to (1, 1)
    TestProvider provider;
    provider.AddItem({geom::Point2D{0.0, 0.0}, 0.6});
    provider.AddGatherer({geom::Point2D{-1.0, -1.0}, geom::Point2D{1.0, 1.0}, 0.6});

    auto events = collision_detector::FindGatherEvents(provider);
    
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].gatherer_id == 0);
    REQUIRE(events[0].item_id == 0);
    REQUIRE(std::abs(events[0].time - 0.5) < EPS);
}

TEST_CASE("FindGatherEvents vertical movement", "[collision_detector]") {
    // Vertical movement from (0, -1) to (0, 1)
    TestProvider provider;
    provider.AddItem({geom::Point2D{0.0, 0.0}, 0.6});
    provider.AddGatherer({geom::Point2D{0.0, -1.0}, geom::Point2D{0.0, 1.0}, 0.6});

    auto events = collision_detector::FindGatherEvents(provider);
    
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].gatherer_id == 0);
    REQUIRE(events[0].item_id == 0);
    REQUIRE(std::abs(events[0].time - 0.5) < EPS);
}

TEST_CASE("FindGatherEvents item exactly at start position", "[collision_detector]") {
    // Item at start position
    TestProvider provider;
    provider.AddItem({geom::Point2D{-1.0, 0.0}, 0.6});
    provider.AddGatherer({geom::Point2D{-1.0, 0.0}, geom::Point2D{1.0, 0.0}, 0.6});

    auto events = collision_detector::FindGatherEvents(provider);
    
    REQUIRE(events.size() == 1);
    REQUIRE(std::abs(events[0].time - 0.0) < EPS);
}

TEST_CASE("FindGatherEvents item exactly at end position", "[collision_detector]") {
    // Item at end position
    TestProvider provider;
    provider.AddItem({geom::Point2D{1.0, 0.0}, 0.6});
    provider.AddGatherer({geom::Point2D{-1.0, 0.0}, geom::Point2D{1.0, 0.0}, 0.6});

    auto events = collision_detector::FindGatherEvents(provider);
    
    REQUIRE(events.size() == 1);
    REQUIRE(std::abs(events[0].time - 1.0) < EPS);
}

TEST_CASE("FindGatherEvents with zero width items and gatherers", "[collision_detector]") {
    // Zero width item and gatherer - only exact path hits
    TestProvider provider;
    provider.AddItem({geom::Point2D{0.0, 0.0}, 0.0});
    provider.AddGatherer({geom::Point2D{-1.0, 0.0}, geom::Point2D{1.0, 0.0}, 0.0});

    auto events = collision_detector::FindGatherEvents(provider);
    
    REQUIRE(events.size() == 1);
    REQUIRE(std::abs(events[0].sq_distance - 0.0) < EPS);
    REQUIRE(std::abs(events[0].time - 0.5) < EPS);
}

TEST_CASE("FindGatherEvents no false positives for zero width", "[collision_detector]") {
    // Zero width item offset from path - should not collect
    TestProvider provider;
    provider.AddItem({geom::Point2D{0.0, 0.1}, 0.0});
    provider.AddGatherer({geom::Point2D{-1.0, 0.0}, geom::Point2D{1.0, 0.0}, 0.0});

    auto events = collision_detector::FindGatherEvents(provider);
    REQUIRE(events.empty());
}

TEST_CASE("FindGatherEvents many items and gatherers", "[collision_detector]") {
    // Many items and gatherers to stress test
    TestProvider provider;
    
    // 5 gatherers at different y positions
    for (int i = 0; i < 5; ++i) {
        double y = static_cast<double>(i) * 0.5;
        provider.AddGatherer({geom::Point2D{-1.0, y}, geom::Point2D{1.0, y}, 0.6});
    }
    
    // 5 items at different y positions (matching gatherers)
    for (int i = 0; i < 5; ++i) {
        double y = static_cast<double>(i) * 0.5;
        provider.AddItem({geom::Point2D{0.0, y}, 0.6});
    }
    
    auto events = collision_detector::FindGatherEvents(provider);
    
    REQUIRE(events.size() == 5);
    // Each gatherer should collect the item at the same y position at time 0.5
    for (const auto& e : events) {
        REQUIRE(e.gatherer_id == e.item_id);
        REQUIRE(std::abs(e.time - 0.5) < EPS);
    }
}

TEST_CASE("FindGatherEvents with no items", "[collision_detector]") {
    // No items
    TestProvider provider;
    provider.AddGatherer({geom::Point2D{-1.0, 0.0}, geom::Point2D{1.0, 0.0}, 0.6});
    
    auto events = collision_detector::FindGatherEvents(provider);
    REQUIRE(events.empty());
}

TEST_CASE("FindGatherEvents with no gatherers", "[collision_detector]") {
    // No gatherers
    TestProvider provider;
    provider.AddItem({geom::Point2D{0.0, 0.0}, 0.6});
    
    auto events = collision_detector::FindGatherEvents(provider);
    REQUIRE(events.empty());
}

TEST_CASE("FindGatherEvents empty provider", "[collision_detector]") {
    // Empty provider
    TestProvider provider;
    
    auto events = collision_detector::FindGatherEvents(provider);
    REQUIRE(events.empty());
}