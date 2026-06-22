#include "collision_detector.h"
#include <cassert>

namespace collision_detector {

CollectionResult TryCollectPoint(geom::Point2D a, geom::Point2D b, geom::Point2D c) {
    // Проверим, что перемещение ненулевое.
    // Тут приходится использовать строгое равенство, а не приближённое,
    // пскольку при сборе заказов придётся учитывать перемещение даже на небольшое
    // расстояние.
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

    // Перебираем всех собирателей
    for (size_t g = 0; g < provider.GatherersCount(); ++g) {
        Gatherer gatherer = provider.GetGatherer(g);

        // Проверяем, было ли перемещение. Если нет - пропускаем (по условию).
        if (gatherer.start_pos.x == gatherer.end_pos.x &&
            gatherer.start_pos.y == gatherer.end_pos.y) {
            continue;
        }

        // Перебираем все предметы
        for (size_t i = 0; i < provider.ItemsCount(); ++i) {
            Item item = provider.GetItem(i);

            // Пытаемся собрать предмет
            CollectionResult result = TryCollectPoint(gatherer.start_pos, gatherer.end_pos, item.position);

            // Радиус сбора — это сумма радиуса собирателя и предмета
            if (result.IsCollected(gatherer.width + item.width)) {
                events.push_back(GatheringEvent{
                    .item_id = i,
                    .gatherer_id = g,
                    .sq_distance = result.sq_distance,
                    .time = result.proj_ratio
                });
            }
        }
    }

    // Сортируем все собранные события в хронологическом порядке (по параметру time)
    std::sort(events.begin(), events.end(), [](const GatheringEvent& a, const GatheringEvent& b) {
        return a.time < b.time;
    });

    return events;
}

}  // namespace collision_detector