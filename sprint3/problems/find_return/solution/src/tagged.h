#pragma once
#include <compare>
#include <string>
#include <functional>

namespace util {

/* * С его помощью можно описать строгий тип на основе другого типа.
 */
template <typename Value, typename Tag>
class Tagged {
public:
    using ValueType = Value;
    using TagType = Tag;

    // Конструктор по умолчанию
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
    
    // Операторы присваивания
    Tagged& operator=(const Value& v) {
        value_ = v;
        return *this;
    }
    
    Tagged& operator=(Value&& v) {
        value_ = std::move(v);
        return *this;
    }

    // Операторы сравнения (критически важно для unordered_map!)
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

    // Оператор сравнения (C++20)
    auto operator<=>(const Tagged&) const = default;

private:
    Value value_{};
};

// Хешер для Tagged-типа
template <typename TaggedValue>
struct TaggedHasher {
    // ВАЖНО: добавлен noexcept для совместимости с GCC 11
    size_t operator()(const TaggedValue& value) const noexcept {
        return std::hash<typename TaggedValue::ValueType>{}(*value);
    }
};

}  // namespace util