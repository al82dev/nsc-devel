#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "nativesharp/Language/ConstantAllocation.h"
#include "nativesharp/Language/ConstantPointer.h"

namespace nativesharp::language
{
class ConstantMemory final
{
public:
    std::optional<ConstantPointer> allocate(
        std::size_t size,
        std::size_t alignment,
        std::string& error);
    std::optional<ConstantPointer> offset(
        ConstantPointer pointer,
        std::ptrdiff_t delta,
        std::string& error) const;
    bool validateDereference(
        ConstantPointer pointer,
        std::size_t width,
        std::string& error) const;
    bool deallocate(std::uint64_t identity, std::string& error);
private:
    std::uint64_t nextIdentity_ = 1;
    std::vector<ConstantAllocation> allocations_;
};
}
