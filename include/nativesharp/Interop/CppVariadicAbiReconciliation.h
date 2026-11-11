#pragma once

#include "nativesharp/Interop/CppInteropSession.h"
#include <span>

namespace nativesharp::interop
{
// The caller must associate each C type with its actual live origin session.
// A null origin cannot authorize a record, including through a pointer.
struct CppVariadicArgumentOrigin
{
    CppType type;
    const CppInteropSession* origin = nullptr;
};

// Resolve against the function's session after checking each record's origin
// and, when necessary, structural equivalence. The caller retains all sessions.
std::optional<abi_plan::VariadicCallPlan> reconcileCppVariadicCallAbiPlan(
    const CppInteropSession& functionSession, std::string_view name,
    std::string_view canonicalType, std::span<const CppVariadicArgumentOrigin> arguments);
}
