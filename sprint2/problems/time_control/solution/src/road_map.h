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
                left = std::min(start.x, end.x) - 0.4;
                right = std::max(start.x, end.x) + 0.4;
                top = start.y - 0.4;
                bottom = start.y + 0.4;
            } else {
                left = start.x - 0.4;
                right = start.x + 0.4;
                top = std::min(start.y, end.y) - 0.4;
                bottom = std::max(start.y, end.y) + 0.4;
            }
        }
        
        bool Contains(double x, double y) const {
            return x >= left - 1e-9 && x <= right + 1e-9 &&
                   y >= top - 1e-9 && y <= bottom + 1e-9;
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