#pragma once

#include "nativesharp/Sema/ConstantValue.h"

#include <optional>
#include <unordered_map>

namespace nativesharp::mir
{
class MirInstruction;
class MirValue;
}

namespace nativesharp::passes
{
class ConstantFolder final
{
public:
    static std::optional<sema::ConstantValue> fold(
        const mir::MirInstruction* instruction,
        const std::unordered_map<const mir::MirValue*, sema::ConstantValue>& known);

    static bool equal(
        const sema::ConstantValue& left,
        const sema::ConstantValue& right) noexcept;
};
}
