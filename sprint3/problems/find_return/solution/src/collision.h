#pragma once

#include "model.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <set>

namespace collision {

// Half-widths
constexpr double PLAYER_HALF_WIDTH = 0.3;   // 0.6 / 2
constexpr double OFFICE_HALF_WIDTH = 0.25;  // 0.5 / 2
constexpr double LOOT_HALF_WIDTH = 0.0;     // zero width

struct CollisionEvent {
    enum Type { LOOT_PICKUP, OFFICE_DELIVERY };
    Type type;
    uint64_t loot_id;
    int loot_type;
    model::Position position;
    double distance;
};

// Check if two circles overlap
inline bool CirclesOverlap(const model::Position& p1, double r1,
                          const model::Position& p2, double r2) {
    double dx = p1.x - p2.x;
    double dy = p1.y - p2.y;
    double dist = std::sqrt(dx * dx + dy * dy);
    return dist <= (r1 + r2);
}

// Check if point is within circle
inline bool IsPointInCircle(const model::Position& point, 
                           const model::Position& center, 
                           double radius) {
    double dx = point.x - center.x;
    double dy = point.y - center.y;
    return (dx * dx + dy * dy) <= (radius * radius);
}

// Find all loot items within pickup radius along a movement path
template<typename LootContainer>
std::vector<CollisionEvent> FindLootCollisions(
    const model::Position& start,
    const model::Position& end,
    const LootContainer& loot_items,
    double pickup_radius = PLAYER_HALF_WIDTH + LOOT_HALF_WIDTH) {
    
    std::vector<CollisionEvent> events;
    
    // If no movement, check only end position
    if (std::abs(start.x - end.x) < 1e-9 && std::abs(start.y - end.y) < 1e-9) {
        for (const auto& [id, item] : loot_items) {
            const auto& [type, pos] = item;
            if (CirclesOverlap(end, pickup_radius, pos, 0.0)) {
                events.push_back({
                    CollisionEvent::LOOT_PICKUP,
                    id,
                    type,
                    pos,
                    std::sqrt(std::pow(end.x - pos.x, 2) + std::pow(end.y - pos.y, 2))
                });
            }
        }
        return events;
    }
    
    // Check along the path
    double dx = end.x - start.x;
    double dy = end.y - start.y;
    double total_dist = std::sqrt(dx * dx + dy * dy);
    
    for (const auto& [id, item] : loot_items) {
        const auto& [type, pos] = item;
        
        // Project loot position onto the path
        double t = ((pos.x - start.x) * dx + (pos.y - start.y) * dy) / (total_dist * total_dist);
        t = std::clamp(t, 0.0, 1.0);
        
        model::Position closest{
            start.x + t * dx,
            start.y + t * dy
        };
        
        double dist_to_loot = std::sqrt(
            std::pow(pos.x - closest.x, 2) + 
            std::pow(pos.y - closest.y, 2)
        );
        
        if (dist_to_loot <= pickup_radius) {
            // Calculate distance along path to this point
            double distance_along_path = t * total_dist;
            
            events.push_back({
                CollisionEvent::LOOT_PICKUP,
                id,
                type,
                pos,
                distance_along_path
            });
        }
    }
    
    // Sort by distance along path
    std::sort(events.begin(), events.end(),
        [](const CollisionEvent& a, const CollisionEvent& b) {
            return a.distance < b.distance;
        });
    
    return events;
}

// Check if a point is within office pickup/delivery radius
inline bool IsNearOffice(const model::Position& player_pos,
                        const model::Office& office,
                        double delivery_radius = PLAYER_HALF_WIDTH + OFFICE_HALF_WIDTH) {
    model::Position office_pos{
        static_cast<double>(office.GetPosition().x + office.GetOffset().dx),
        static_cast<double>(office.GetPosition().y + office.GetOffset().dy)
    };
    return IsPointInCircle(player_pos, office_pos, delivery_radius);
}

} // namespace collision