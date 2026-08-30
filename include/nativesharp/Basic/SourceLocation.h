#pragma once

#include "nativesharp/Basic/FileId.h"

#include <cstdint>

namespace nativesharp
{
class SourceLocation final
{
public:
    constexpr SourceLocation() noexcept = default;

    constexpr SourceLocation(FileId fileId, std::uint32_t offset) noexcept
        : fileId_(fileId),
          offset_(offset)
    {
    }

    constexpr FileId fileId() const noexcept
    {
        return fileId_;
    }

    constexpr std::uint32_t offset() const noexcept
    {
        return offset_;
    }

    constexpr bool isValid() const noexcept
    {
        return fileId_.isValid();
    }

    friend constexpr bool operator==(SourceLocation left, SourceLocation right) noexcept = default;

private:
    FileId fileId_;
    std::uint32_t offset_ = 0;
};
}
