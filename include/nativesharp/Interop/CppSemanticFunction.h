#pragma once

#include "nativesharp/Frontend/NamespaceFunctionSymbol.h"
#include "nativesharp/Frontend/CppImportedInstanceMethod.h"
#include "nativesharp/Frontend/CppImportedStaticMethod.h"
#include "nativesharp/Interop/CppFunctionDeclaration.h"
#include "nativesharp/Interop/CppRecordDeclaration.h"
#include "nativesharp/Interop/CppScalarTypeMapping.h"
#include "nativesharp/Interop/CppProjectSessions.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <variant>

namespace nativesharp::frontend {
class ProjectMemberModel;
class ProjectOopMirModel;
struct ObjectConstructionBinding;
struct ClassDestructionPlan;
}

namespace nativesharp::interop
{
struct CppRecordGroups;
struct CppCRecordSnapshot;
namespace metadata_detail { struct CDependencyFunctionSnapshot; }
struct CppSemanticFunctionFailure
{
    std::string message;
};

using CppSemanticFunctionResult =
    std::variant<frontend::NamespaceFunctionSymbol, CppSemanticFunctionFailure>;

// Maps a stored C function pointer using the same prototype rules as function
// signatures; preserve storage qualifiers and reject unrepresentable prototypes.
std::optional<CppScalarTypeMapping> projectCppStoredFunctionPointer(
    const CppType& type, std::optional<std::size_t> projectOrdinal = std::nullopt,
    std::string_view moduleName = {}, const frontend::ProjectNameLookup* lookup = nullptr,
    bool cppRecordValues = false);

// Projects externally named free functions using representable scalar types and supported
// 64-bit pointers, including parameters to const scalars, into the NSC name index. This does
// not classify the full call ABI or implement C++ overload resolution.
CppSemanticFunctionResult projectCppSemanticFunction(
    const CppFunctionDeclaration& function, std::size_t projectOrdinal, std::string_view moduleName);

// Isolated preparation only: a successful result does not make the record available to
// the production frontend or establish object layout, lifetime, or call ABI support.
CppSemanticFunctionResult projectCppSemanticFunctionWithRecords(
    const CppFunctionDeclaration& function, std::size_t projectOrdinal, std::string_view moduleName,
    const CppRecordGroups& records, const frontend::ProjectNameLookup* lookup = nullptr);
// Reprojects a saved C declaration through the same type and ABI mapping used
// by live Clang observations. It does not install or link the resulting symbol.
CppSemanticFunctionResult projectCppPrecompiledCFunction(
    const metadata_detail::CDependencyFunctionSnapshot& function,
    std::span<const CppCRecordSnapshot> records, std::size_t projectOrdinal,
    std::string_view moduleName, std::string_view targetTriple,
    const frontend::ProjectNameLookup& lookup);

struct CppSemanticInstanceMethod
{
    std::size_t projectOrdinal;
    std::string ownerIdentityKey;
    std::string qualifiedName;
    std::string externalSymbol;
    std::string canonicalFunctionType;
    bool receiverConst;
    CppScalarTypeMapping result;
    std::vector<CppScalarTypeMapping> parameters;
    bool dllImport = false;
};
using CppSemanticInstanceMethodResult = std::variant<CppSemanticInstanceMethod, CppSemanticFunctionFailure>;
// Experimental metadata only, not a bound method, calling convention proof or exception policy.
CppSemanticInstanceMethodResult projectCppSemanticInstanceMethod(
    const CppRecordDeclaration& record, std::size_t methodIndex,
    std::size_t projectOrdinal, std::string_view moduleName, std::string_view targetTriple,
    const CppRecordGroups* records = nullptr);
frontend::CppImportedInstanceMethod toCppImportedInstanceMethod(const CppSemanticInstanceMethod& method);

struct CppSemanticStaticMethod
{
    std::size_t projectOrdinal;
    std::string ownerIdentityKey;
    std::string qualifiedName;
    std::string externalSymbol;
    std::string canonicalFunctionType;
    CppScalarTypeMapping result;
    std::vector<CppScalarTypeMapping> parameters;
    bool dllImport = false;
};
using CppSemanticStaticMethodResult = std::variant<CppSemanticStaticMethod, CppSemanticFunctionFailure>;
// Isolated metadata, not proof of a complete static-method ABI or exception implementation.
CppSemanticStaticMethodResult projectCppSemanticStaticMethod(
    const CppRecordDeclaration& record, std::size_t methodIndex,
    std::size_t projectOrdinal, std::string_view moduleName, std::string_view targetTriple,
    const CppRecordGroups* records = nullptr);
frontend::CppImportedStaticMethod toCppImportedStaticMethod(const CppSemanticStaticMethod& method);
bool provideExperimentalCppStaticMethods(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup,
    std::vector<frontend::CppImportedStaticMethod>& output, std::string& error);

enum class CppLifetimeReturnAbi
{
    ThisPointer,
    Void
};

struct CppSemanticLifetimeMethod
{
    std::size_t projectOrdinal;
    std::string ownerIdentityKey;
    CppMethodKind kind;
    std::string externalSymbol;
    std::string canonicalFunctionType;
    std::vector<CppScalarTypeMapping> parameters;
    CppLifetimeReturnAbi returnAbi;
    bool dllImport;
    bool isNonThrowing;
};
using CppSemanticLifetimeMethodResult = std::variant<CppSemanticLifetimeMethod, CppSemanticFunctionFailure>;
// Metadata only: no storage, construction, destruction, or unwinding is implied.
CppSemanticLifetimeMethodResult projectCppSemanticLifetimeMethod(
    const CppRecordDeclaration& record, std::size_t methodIndex,
    std::size_t projectOrdinal, std::string_view moduleName, std::string_view targetTriple);
frontend::CppImportedLifetimeMethod toCppImportedLifetimeMethod(const CppSemanticLifetimeMethod& method);

struct CppDeferredLifetimeMethod
{
    std::size_t methodIndex;
    CppMethodKind kind;
    std::string reason;
};
struct CppSemanticLifetimeFamily
{
    std::size_t projectOrdinal;
    std::string ownerIdentityKey;
    std::vector<CppSemanticLifetimeMethod> methods;
    std::vector<CppDeferredLifetimeMethod> deferred;
    bool hasProjectedSignatureCollision = false;
    std::optional<CppImplicitLifetimeMembers> implicitMembers;
    // Null means the owning session could not provide a complete observation.
    std::optional<std::vector<CppImplicitLifetimeDeclaration>> implicitDeclarations;
};
// The snapshot contains observed explicit declarations only; it does not certify implicit
// special members, overload resolution, or safe object construction and destruction.
bool collectCppSemanticLifetimeFamilies(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup,
    std::vector<CppSemanticLifetimeFamily>& output, std::string& error);
bool provideExperimentalCppLifetimeMethods(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup,
    std::vector<frontend::CppImportedLifetimeMethod>& output, std::string& error);
struct CppImplicitLifetimeEmissionRequest
{
    std::size_t projectOrdinal;
    std::size_t memberOrdinal;
    std::string qualifiedType;
    std::string abiSymbol;
    CppImplicitLifetimeKind kind;
    // Non-owning; the originating ProjectCompilation must outlive this plan.
    const CppInteropSession* session;
};
bool planCppImplicitLifetimeEmission(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup, const frontend::ProjectMemberModel& members,
    std::span<const std::size_t> selectedMembers,
    std::vector<CppImplicitLifetimeEmissionRequest>& output, std::string& error);
bool planCppImplicitLifetimeFromBindings(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup, const frontend::ProjectMemberModel& members,
    std::span<const frontend::ObjectConstructionBinding> constructions,
    std::span<const frontend::ClassDestructionPlan> destructions,
    std::vector<CppImplicitLifetimeEmissionRequest>& output, std::string& error);
bool planCppImplicitLifetimeFromValues(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup, const frontend::ProjectMemberModel& members,
    std::span<const frontend::ObjectConstructionBinding> constructions,
    const frontend::ProjectOopMirModel& mir,
    std::vector<CppImplicitLifetimeEmissionRequest>& output, std::string& error);
bool provideExperimentalCppInstanceMethods(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup,
    std::vector<frontend::CppImportedInstanceMethod>& output, std::string& error);
}
