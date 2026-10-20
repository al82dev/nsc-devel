#pragma once

#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace nativesharp::llvm_backend
{
class IdentityWriter final
{
public:
    IdentityWriter();

    template <typename Value>
    void scalar(Value value)
    {
        if constexpr (std::is_enum_v<Value>)
            appendInteger(static_cast<std::underlying_type_t<Value>>(value));
        else if constexpr (std::is_same_v<std::remove_cv_t<Value>, bool>)
            output_.push_back(value ? '1' : '0');
        else
            appendInteger(value);
        output_.push_back(';');
    }

    void text(std::string_view value);

    template <typename Value>
    void optional(const std::optional<Value>& value)
    {
        scalar(value.has_value());
        if (value.has_value()) scalar(*value);
    }

    template <typename Value>
    void scalars(const std::vector<Value>& values)
    {
        scalar(values.size());
        for (const Value& value : values) scalar(value);
    }

    void texts(const std::vector<std::string>& values);
    std::string take();

private:
    template <typename Value>
    void appendInteger(Value value)
    {
        char buffer[32];
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
        output_.append(buffer, result.ptr);
    }

    std::string output_;
};
}
