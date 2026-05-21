#pragma once
#include <compare>
#include <string>
#include <functional>

namespace util {

/**
 * Вспомогательный шаблонный класс "Маркированный тип".
 * С его помощью можно описать строгий тип на основе другого типа.
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

    // Операторы сравнения (важно для unordered_map!)
    bool operator==(const Tagged& other) const {
        return value_ == other.value_;
    }
    
    bool operator!=(const Tagged& other) const {
        return value_ != other.value_;
    }
    
    bool operator==(const Value& other) const {
        return value_ == other;
    }
    
    bool operator!=(const Value& other) const {
        return value_ != other;
    }

    // Оператор сравнения (C++20)
    auto operator<=>(const Tagged&) const = default;

private:
    Value value_{};
};

// Хешер для Tagged-типа
template <typename TaggedValue>
struct TaggedHasher {
    size_t operator()(const TaggedValue& value) const {
        return std::hash<typename TaggedValue::ValueType>{}(*value);
    }
};

}  // namespace util