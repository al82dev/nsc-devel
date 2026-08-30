#pragma once

#include "nativesharp/Basic/SourceLocation.h"

namespace nativesharp
{
class SourceRange final
{
public:
    constexpr SourceRange() noexcept = default;

    constexpr SourceRange(SourceLocation begin, SourceLocation end) noexcept
        : begin_(begin),
          end_(end)
    {
    }

    constexpr SourceLocation begin() const noexcept
    {
        return begin_;
    }

    constexpr SourceLocation end() const noexcept
    {
        return end_;
    }

    constexpr bool isValid() const noexcept
    {
        return begin_.isValid() && end_.isValid() && begin_.fileId() == end_.fileId() &&
            begin_.offset() <= end_.offset();
    }

private:
    SourceLocation begin_;
    SourceLocation end_;
};
}
