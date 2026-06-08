#pragma once

#include <compare>
#include <functional>

namespace util {

template <typename ValueType, typename Tag>
class Tagged {
public:
    using ValueTypeT = ValueType;

    explicit Tagged(ValueType value) : value_(std::move(value)) {}

    const ValueType& GetUnderlying() const noexcept { return value_; }

    ValueType& GetUnderlying() noexcept { return value_; }

    operator ValueType() const noexcept { return value_; }

    auto operator<=>(const Tagged& other) const = default;

private:
    ValueType value_;
};

} // namespace util

namespace std {

template <typename ValueType, typename Tag>
struct hash<util::Tagged<ValueType, Tag>> {
    size_t operator()(const util::Tagged<ValueType, Tag>& tagged) const noexcept {
        return hash<ValueType>{}(tagged.GetUnderlying());
    }
};

} // namespace std