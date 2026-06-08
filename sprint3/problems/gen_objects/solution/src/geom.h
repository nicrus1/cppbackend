#pragma once

namespace model {

struct Point {
    double x = 0;
    double y = 0;
};

struct Size {
    double width = 0;
    double height = 0;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    double dx = 0;
    double dy = 0;
};

} // namespace model