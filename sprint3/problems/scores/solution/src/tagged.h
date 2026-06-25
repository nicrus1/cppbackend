#pragma once
#include <compare>
#include <string>
#include <functional>

namespace util {

template <typename Value, typename Tag>
class Tagged {
public:
    using ValueType = Value;
    using TagType = Tag;

    Tagged() = default;
    
    explicit Tagged(Value&& v)
        : value_(std::move(v)) {
    }
    
    explicit Tagged(const Value& v)
        : value_(v) {
    }

    const Value& operator*() const {
        return value_;
    }

    Value& operator*() {
        return value_;
    }
    
    const Value* operator->() const {
        return &value_;
    }
    
    Value* operator->() {
        return &value_;
    }
    
    Tagged& operator=(const Value& v) {
        value_ = v;
        return *this;
    }
    
    Tagged& operator=(Value&& v) {
        value_ = std::move(v);
        return *this;
    }

    friend bool operator==(const Tagged& lhs, const Tagged& rhs) {
        return lhs.value_ == rhs.value_;
    }
    
    friend bool operator!=(const Tagged& lhs, const Tagged& rhs) {
        return lhs.value_ != rhs.value_;
    }
    
    friend bool operator==(const Tagged& lhs, const Value& rhs) {
        return lhs.value_ == rhs;
    }
    
    friend bool operator!=(const Tagged& lhs, const Value& rhs) {
        return lhs.value_ != rhs;
    }

    auto operator<=>(const Tagged&) const = default;

private:
    Value value_{};
};

template <typename TaggedValue>
struct TaggedHasher {
    size_t operator()(const TaggedValue& value) const {
        return std::hash<typename TaggedValue::ValueType>{}(*value);
    }
};

}  // namespace util