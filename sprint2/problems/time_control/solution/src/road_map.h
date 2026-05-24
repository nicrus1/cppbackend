#pragma once
#include "model.h"
#include <vector>
#include <optional>
#include <algorithm>

namespace model {

class RoadMap {
public:
    struct RoadSegment {
        Road road;
        double left, right, top, bottom;
        
        RoadSegment(const Road& r) : road(r) {
            auto start = r.GetStart();
            auto end = r.GetEnd();
            
            if (r.IsHorizontal()) {
                left = std::min(start.x, end.x);
                right = std::max(start.x, end.x);
                top = start.y;
                bottom = start.y;
            } else {
                left = start.x;
                right = start.x;
                top = std::min(start.y, end.y);
                bottom = std::max(start.y, end.y);
            }
        }
        
        bool Contains(double x, double y) const {
            if (road.IsHorizontal()) {
                return x >= left - 1e-9 && x <= right + 1e-9 &&
                       std::abs(y - top) <= 0.4 + 1e-9;
            } else {
                return y >= top - 1e-9 && y <= bottom + 1e-9 &&
                       std::abs(x - left) <= 0.4 + 1e-9;
            }
        }
    };
    
    void AddRoad(const Road& road) {
        roads_.push_back(RoadSegment(road));
    }
    
    const RoadSegment* FindRoad(double x, double y) const {
        for (const auto& seg : roads_) {
            if (seg.Contains(x, y)) {
                return &seg;
            }
        }
        return nullptr;
    }

private:
    std::vector<RoadSegment> roads_;
};

} // namespace model