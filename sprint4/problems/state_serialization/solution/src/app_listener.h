#pragma once

#include <chrono>

namespace app {

using milliseconds = std::chrono::milliseconds;

class ApplicationListener {
public:
    virtual ~ApplicationListener() = default;
    
    virtual void OnTick(milliseconds delta) = 0;
    
    virtual void OnShutdown() = 0;
};

}  // namespace app