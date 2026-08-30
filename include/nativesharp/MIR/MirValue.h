#pragma once

#include <cstdint>

namespace nativesharp::sema
{
class Type;
}

namespace nativesharp::mir
{
class MirValue final
{
public:
    MirValue(std::uint32_t id, sema::Type* type);

    std::uint32_t id() const noexcept;
    sema::Type* type() const noexcept;

private:
    std::uint32_t id_;
    sema::Type* type_;
};
}
