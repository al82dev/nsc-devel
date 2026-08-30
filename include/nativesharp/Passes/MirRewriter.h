#pragma once

#include <cstddef>

namespace nativesharp::mir
{
class MirBlock;
class MirModule;
class MirValue;
}

namespace nativesharp::passes
{
class MirRewriter final
{
public:
    static std::size_t replaceAllUses(
        mir::MirModule* module,
        const mir::MirValue* from,
        mir::MirValue* to);

    static std::size_t instructionCount(const mir::MirModule* module) noexcept;
    static std::size_t blockCount(const mir::MirModule* module) noexcept;
};
}
