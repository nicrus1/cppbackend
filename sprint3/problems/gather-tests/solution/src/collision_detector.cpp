#include "collision_detector.h"
#include <cassert>
#include <algorithm>

namespace collision_detector {

CollectionResult TryCollectPoint(geom::Point2D a, geom::Point2D b, geom::Point2D c) {
    // Проверим, что перемещение ненулевое.
    assert(b.x != a.x || b.y != a.y);
    const double u_x = c.x - a.x;
    const double u_y = c.y - a.y;
    const double v_x = b.x - a.x;
    const double v_y = b.y - a.y;
    const double u_dot_v = u_x * v_x + u_y * v_y;
    const double u_len2 = u_x * u_x + u_y * u_y;
    const double v_len2 = v_x * v_x + v_y * v_y;
    const double proj_ratio = u_dot_v / v_len2;
    const double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

    return CollectionResult(sq_distance, proj_ratio);
}

std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider) {
    std::vector<GatheringEvent> events;

    for (size_t g = 0; g < provider.GatherersCount(); ++g) {
        Gatherer gatherer = provider.GetGatherer(g);
        
        // Если собиратель не сдвинулся с места, он ничего не подберет
        if (gatherer.start_pos.x == gatherer.end_pos.x && 
            gatherer.start_pos.y == gatherer.end_pos.y) {
            continue;
        }

        for (size_t i = 0; i < provider.ItemsCount(); ++i) {
            Item item = provider.GetItem(i);
            
            CollectionResult result = TryCollectPoint(gatherer.start_pos, gatherer.end_pos, item.position);
            
            // Расстояние взаимодействия — это сумма радиусов/ширин собирателя и предмета
            double collect_radius = gatherer.width + item.width;

            if (result.IsCollected(collect_radius)) {
                GatheringEvent evt;
                evt.item_id = i;
                evt.gatherer_id = g;
                evt.sq_distance = result.sq_distance;
                evt.time = result.proj_ratio;
                events.push_back(evt);
            }
        }
    }

    // События должны быть отсортированы по времени
    std::sort(events.begin(), events.end(),
              [](const GatheringEvent& lhs, const GatheringEvent& rhs) {
                  return lhs.time < rhs.time;
              });

    return events;
}

}  // namespace collision_detector