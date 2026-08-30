#pragma once

#include <cstdint>

namespace nativesharp
{
class FileId final
{
public:
    constexpr FileId() noexcept = default;
    explicit constexpr FileId(std::uint32_t value) noexcept
        : value_(value)
    {
    }

    constexpr bool isValid() const noexcept
    {
        return value_ != 0;
    }

    constexpr std::uint32_t value() const noexcept
    {
        return value_;
    }

    friend constexpr bool operator==(FileId left, FileId right) noexcept = default;

private:
    std::uint32_t value_ = 0;
};
}
