#pragma once

#include "nativesharp/Interop/CppInteropSession.h"
#include "nativesharp/Interop/CppVariadicAbiValidation.h"
#include <span>

namespace nativesharp::compilation { class ProjectGraph; }
namespace nativesharp::frontend { struct NamespaceFunctionSymbol; class ProjectNameLookup; struct PrecompiledSyntaxModule; struct CppImportedTypeAlias; }
namespace nativesharp::interop
{
struct CppProjectSessions;
struct CppProjectSessionEnvironment;
class CppInteropSessionFactory;
// The caller must associate each C type with its actual live origin session.
// A null origin cannot authorize a record, including through a pointer.
struct CppVariadicArgumentOrigin
{
    CppType type;
    const CppInteropSession* origin = nullptr;
};

// Experimental observation only: the caller supplies the actual origin of each
// signature type. A returned synthetic plan is not authorization to emit.
std::optional<abi_plan::Plan> reconcileCppFunctionPointerAbiPlan(
    const CppInteropSession& aliasSession, std::string_view name, const CppType& expectedPointer,
    const CppVariadicArgumentOrigin& result, std::span<const CppVariadicArgumentOrigin> parameters);

// A resolved live alias is required. This does not authorize lowering or replay.
std::shared_ptr<const abi_plan::Plan> planCppFunctionPointerForAlias(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup, const frontend::CppImportedTypeAlias& alias,
    std::string_view resolvedTypeKey);

// Reconstruct the observed record and pointer type from the current catalogue;
// a fresh Clang session classifies it after identity and layout checks.
std::shared_ptr<const abi_plan::Plan> planPrecompiledCppFunctionPointerForAlias(
    const compilation::ProjectGraph& graph, std::span<const frontend::PrecompiledSyntaxModule> modules,
    const frontend::ProjectNameLookup& lookup, const frontend::CppImportedTypeAlias& alias,
    std::string_view resolvedTypeKey, const CppProjectSessionEnvironment& environment,
    CppInteropSessionFactory& factory);

// Resolve against the function's session after checking each record's origin
// and, when necessary, structural equivalence. The caller retains all sessions.
std::optional<abi_plan::VariadicCallPlan> reconcileCppVariadicCallAbiPlan(
    const CppInteropSession& functionSession, std::string_view name,
    std::string_view canonicalType, std::span<const CppVariadicArgumentOrigin> arguments);

// Experimental observation: the typedef must match its live session and each
// record argument must be reconciled with its actual origin before querying Clang.
std::optional<abi_plan::VariadicCallPlan> reconcileCppVariadicFunctionPointerCallAbiPlan(
    const CppInteropSession& aliasSession, std::string_view name, const CppType& expectedPointer,
    std::span<const CppVariadicArgumentOrigin> arguments);

// Records are accepted only after resolving and comparing their live origins.
std::shared_ptr<const abi_plan::VariadicCallPlan> planCppVariadicCall(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup, const frontend::NamespaceFunctionSymbol& function,
    std::span<const std::string> optionalTypeKeys);

// Uses only verified producer catalogues and a fresh Clang session; callers
// serialize factory use and keep the session environment alive for the call.
std::shared_ptr<const abi_plan::VariadicCallPlan> planPrecompiledCppVariadicCall(
    const compilation::ProjectGraph& graph, std::span<const frontend::PrecompiledSyntaxModule> modules,
    const frontend::ProjectNameLookup& lookup, const frontend::NamespaceFunctionSymbol& function,
    std::span<const std::string> optionalTypeKeys, const CppProjectSessionEnvironment& environment,
    CppInteropSessionFactory& factory);
}
