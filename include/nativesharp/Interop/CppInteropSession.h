#pragma once

#include "nativesharp/Interop/CppSessionDiagnostic.h"
#include "nativesharp/Interop/CppSessionOptions.h"
#include "nativesharp/Interop/CppFunctionDeclaration.h"
#include "nativesharp/Interop/CppGlobalDeclaration.h"
#include "nativesharp/Interop/CppNamespaceDeclaration.h"
#include "nativesharp/Interop/CppRecordDeclaration.h"
#include "nativesharp/Interop/CppTypeAliasDeclaration.h"
#include "nativesharp/Interop/CppEnumDeclaration.h"
#include "nativesharp/Interop/CppAbiPlan.h"
#include "nativesharp/Basic/Sha256.h"

#include <string>
#include <string_view>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace nativesharp::interop
{
enum class CppSessionState { Ready, Closing, Closed };

struct CppSessionCloseResult
{
    bool succeeded = true;
    std::string message;
};

struct CppSourceFileContent final
{
    std::filesystem::path path;
    std::uintmax_t size = 0;
    basic::Sha256Digest digest{};
};

enum class CppImplicitLifetimeKind { DefaultConstructor, CopyConstructor, MoveConstructor, Destructor };
enum class CppExceptionSpecification { Unresolved, NonThrowing, PotentiallyThrowing };
struct CppImplicitLifetimeDeclaration
{
    CppImplicitLifetimeKind kind;
    std::string canonicalFunctionType;
    std::vector<CppFunctionParameter> parameters;
    CppAccess access;
    bool isDeleted;
    bool isTrivial;
    CppExceptionSpecification exceptionSpecification;
    // ABI name only: declaration does not imply an emitted or linkable definition.
    std::optional<std::string> abiSymbol;
    bool dllImport = false;
};

struct CppCInlineIRExport
{
    std::string name;
    std::string symbol;
};

// Implementations own their compiler state. Snapshots remain valid after close.
// close() is idempotent; no caller may use implementation-specific state after it.
// Callers must keep the session alive during every call and coordinate destruction.
// The Clang implementation serializes calls on the same live session; callers must
// not assume concurrent factory opens or other implementations have that guarantee.
class CppInteropSession
{
public:
    virtual ~CppInteropSession() = default;
    virtual CppSessionState state() const noexcept = 0;
    virtual CppSessionOptions optionsSnapshot() const = 0;
    virtual std::vector<CppSessionDiagnostic> diagnosticsSnapshot() const = 0;
    // Effective Clang language option, not an inference from the target triple.
    // nullopt means unavailable or that the session has closed.
    virtual std::optional<bool> microsoftVolatileSemantics() const { return std::nullopt; }
    // Empty after close; the count is a control query, not a C++ import API.
    virtual std::optional<std::size_t> topLevelDeclarationCount() const = 0;
    // Initial projection: concrete free functions in namespaces and linkage specifications.
    // nullopt means closed; returned values own their data independently of the session.
    virtual std::optional<std::vector<CppFunctionDeclaration>> freeFunctionsSnapshot() const = 0;
    // Experimental C-only query: the name and canonical type select one declaration in this session.
    // The returned value owns its data; nullopt also covers unsupported ABI plans and closed sessions.
    virtual std::optional<abi_plan::Plan> cFunctionAbiPlan(std::string_view, std::string_view) const
    { return std::nullopt; }
    // Types must come from this session and describe already-promoted optional
    // arguments. Identity and layout checks are not proof of cross-session ODR
    // equivalence. nullopt includes a closed session or unsupported form.
    virtual std::optional<abi_plan::VariadicCallPlan> cVariadicCallAbiPlan(
        std::string_view, std::string_view, const std::vector<CppType>&) const
    { return std::nullopt; }
    // Initial projection of named, non-dependent records; nullopt means closed.
    virtual std::optional<std::vector<CppRecordDeclaration>> recordsSnapshot() const = 0;
    // Live C AST observation only: true means a unique, complete, plain record
    // with unqualified builtin or named-record fields, fixed arrays or
    // attribute-free typedefs or named bitfields, and no declaration attributes. This observation
    // alone is not an ABI-equivalence proof.
    virtual std::optional<bool> cRecordHasPlainReplaySyntax(std::string_view) const { return std::nullopt; }
    // As above, for a C record with only Clang's packed attribute.
    virtual std::optional<bool> cRecordHasPackedReplaySyntax(std::string_view) const { return std::nullopt; }
    // A missing value means this session cannot provide an alias snapshot.
    virtual std::optional<std::vector<CppTypeAliasDeclaration>> typeAliasesSnapshot() const { return std::nullopt; }
    // Includes anonymous enums; this snapshot does not imply semantic import.
    virtual std::optional<std::vector<CppEnumDeclaration>> enumsSnapshot() const { return std::nullopt; }
    // Requests declaration from Clang Sema; no body or linkable symbol is implied.
    // nullopt also covers unavailable Sema, ambiguous names and unsupported records.
    virtual std::optional<std::vector<CppImplicitLifetimeDeclaration>> implicitLifetimeDeclarations(
        std::string_view) const { return std::nullopt; }
    // Experimental isolated IR only; no object, linkage, or lifetime integration is implied.
    virtual std::optional<std::string> emitImplicitLifetimeIR(
        std::string_view, CppImplicitLifetimeKind) const { return std::nullopt; }
    // Isolated C static-inline IR; the caller owns symbol uniqueness and linkage integration.
    virtual std::optional<std::string> emitCStaticInlineIR(
        const std::vector<CppCInlineIRExport>&,
        const std::vector<CppCInlineIRExport>& = {}) const { return std::nullopt; }
    // Experimental structural comparison only; nullopt means no reliable comparison was possible.
    // A true result is not proof of full ODR or ABI equivalence.
    virtual std::optional<bool> structurallyEquivalentRecord(
        const CppInteropSession&, std::string_view) const { return std::nullopt; }
    // Named namespace/file variables; nullopt means closed.
    virtual std::optional<std::vector<CppGlobalDeclaration>> globalsSnapshot() const = 0;
    // Includes empty and anonymous namespaces; nullopt means closed.
    virtual std::optional<std::vector<CppNamespaceDeclaration>> namespacesSnapshot() const = 0;
    // File names observed by Clang's SourceManager, not a complete cache identity:
    // absent-header probes, virtual buffers and concurrent file changes are not captured.
    virtual std::optional<std::vector<std::filesystem::path>> sourceFilesSnapshot() const = 0;
    // Hashes already-loaded Clang buffers; nullopt also means a buffer is unavailable.
    // This is not a full cache key or a snapshot of files changing concurrently.
    virtual std::optional<std::vector<CppSourceFileContent>> sourceFileContentsSnapshot() const = 0;
    // Fingerprint of the expanded tokens and observed preprocessor decisions.
    // It must be combined with options, file contents and toolchain identity;
    // by itself it is not a complete or validated cache key. nullopt also denotes
    // an unstable builtin such as __TIME__, __DATE__ or __TIMESTAMP__.
    virtual std::optional<basic::Sha256Digest> preprocessingFingerprint() const { return std::nullopt; }
    virtual CppSessionCloseResult close() = 0;
};

struct CppSessionOpenFailure
{
    std::string phase;
    std::string message;
    std::vector<CppSessionDiagnostic> diagnostics;
};

// A successful alternative must contain a non-null, ready session.
using CppSessionOpenResult =
    std::variant<std::unique_ptr<CppInteropSession>, CppSessionOpenFailure>;
}
