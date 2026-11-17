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

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace nativesharp::interop
{
enum class CppSessionState { Ready, Closing, Closed };
// EN: T17-008. The debug information of C++ code that NSC emits from a session.
// ES: T17-008. La informacion de depuracion del codigo C++ que NSC emite desde una sesion.
enum class CppDebugInformation { None, LineTables, Full };

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

enum class CppSpecialMemberSelectionStatus { Selected, MissingOrDeleted, Ambiguous };
struct CppSpecialMemberSelection
{
    CppSpecialMemberSelectionStatus status = CppSpecialMemberSelectionStatus::MissingOrDeleted;
    // Declared member access, not a context-dependent accessibility decision.
    std::optional<CppAccess> access;
    std::optional<bool> isDeleted;
    std::optional<bool> isTrivial;
};
struct CppRecordValueOperations
{
    CppSpecialMemberSelection copyFromConst;
    CppSpecialMemberSelection copyFromMutable;
    CppSpecialMemberSelection destructor;
    CppSpecialMemberSelection defaultConstructor;
    CppSpecialMemberSelection assignFromConst;
    CppSpecialMemberSelection assignFromMutable;
    std::optional<std::string> clangUsr;
};

// Shared by live and reconstructed observations. Identity, origin, target and
// layout must be verified separately by the caller.
inline bool cppRecordValueOperationsEligible(const CppRecordValueTraits& traits,
                                             const CppRecordValueOperations& operations) noexcept
{
    // EN: T12-001. A trivially copyable record with trivial default construction and destruction is a value
    //     whose bytes are the object, also when it is not standard layout (fields in a base and in the
    //     derived record, mixed access). The call ABI of such a value is still Clang's for each function.
    // ES: T12-001. Un record trivialmente copiable con construccion por defecto y destruccion triviales es un
    //     valor cuyos bytes son el objeto, tambien si no es de disposicion estandar (campos en una base y en
    //     el derivado, acceso mixto). El ABI de llamada de ese valor sigue siendo el de Clang en cada funcion.
    if (traits.hasNonLiteralOrVolatileFieldsOrBases || !traits.triviallyCopyable ||
        !traits.trivialDefaultConstructor || !traits.trivialDestructor) return false;
    const auto valid = [](const CppSpecialMemberSelection& operation) {
        return operation.status == CppSpecialMemberSelectionStatus::Selected &&
            operation.access == CppAccess::Public && operation.isDeleted == false &&
            operation.isTrivial == true;
    };
    return valid(operations.copyFromConst) && valid(operations.copyFromMutable) &&
        valid(operations.destructor) && valid(operations.defaultConstructor) &&
        valid(operations.assignFromConst) && valid(operations.assignFromMutable);
}

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
// EN: T13-006. A template argument NativeSharp supplies: a builtin C++ type, a record or enum by its Clang
//     identity, either with pointers, or an integral or bool constant for a non-type parameter.
// ES: T13-006. Un argumento de plantilla que aporta NativeSharp: un tipo C++ basico, un record o enum por su
//     identidad de Clang, con punteros, o una constante integral o bool para un parametro no tipo.
// EN: T13-007, T13-008. A trivial NativeSharp struct given as a C++ template argument: the session declares a
//     C++ struct with the same fields, in order, in namespace `__nsc::<module>::<namespace>`, which NativeSharp
//     maps back to the struct itself. A field is a builtin type, a pointer, or another such struct.
// ES: T13-007, T13-008. Un struct NativeSharp trivial dado como argumento de plantilla C++: la sesion declara un
//     struct C++ con los mismos campos, en orden, en el namespace `__nsc::<modulo>::<namespace>`, que
//     NativeSharp vuelve a asociar con el propio struct. Un campo es un tipo basico, un puntero u otro struct asi.
struct CppNativeStruct;
struct CppNativeStructField
{
    std::string name;
    std::string builtin;
    std::shared_ptr<const CppNativeStruct> structure;
    unsigned pointerDepth = 0;
};
struct CppNativeStruct
{
    std::string moduleName;
    std::vector<std::string> namespaceSegments;
    std::string name;
    std::vector<CppNativeStructField> fields;
};
inline constexpr std::string_view cppNativeStructNamespace = "__nsc";
// T13-007: the layout Clang gives a mirror, checked against NativeSharp's. / El layout del espejo segun Clang.
struct CppNativeStructLayout
{
    std::string moduleName;
    std::vector<std::string> namespaceSegments;
    std::string name;
    std::uint64_t sizeBytes = 0;
    std::uint64_t alignmentBytes = 0;
    std::vector<std::uint64_t> fieldOffsets;
};

struct CppTemplateArgument
{
    enum class Kind { Type, Integral, Boolean, Template, NativeStruct };
    Kind kind = Kind::Type;
    std::string builtin;
    std::optional<std::string> recordUsr;
    unsigned pointerDepth = 0;
    std::int64_t integral = 0;
    bool boolean = false;
    // T13-013: a class template for a template template parameter. / Una plantilla de clase como argumento.
    std::string templateName;
    // T13-005: a call argument passed as an lvalue (`ref`, `in`). / Un argumento de llamada pasado como lvalue.
    bool lvalue = false;
    // T13-007: the NativeSharp struct of a NativeStruct argument. / El struct NativeSharp del argumento.
    std::shared_ptr<const CppNativeStruct> nativeStruct;
};

// T13-010: the specialization Sema produced, or Clang's diagnostics. / La especializacion de Sema, o sus diagnosticos.
// EN: T19-005. The type an alias template (`template<class X> using A = ...;`) names with some arguments, written
//     as NativeSharp names it: a builtin scalar spelled as C++ does, or a record or enum by its namespaces and
//     enclosing records and its name (a class template specialization by its NativeSharp name); then pointers.
// ES: T19-005. El tipo que nombra una plantilla alias con unos argumentos, escrito como lo nombra NativeSharp: un
//     escalar predefinido con la ortografia de C++, o un record o enum por sus namespaces y records contenedores y
//     su nombre (una especializacion de plantilla de clase por su nombre NativeSharp); luego los punteros.
struct CppTemplateAliasTarget
{
    std::string builtin;
    std::vector<std::string> path;
    unsigned pointerDepth = 0;
};

struct CppTemplateInstantiation
{
    bool instantiated = false;
    std::string name;
    std::optional<std::string> clangUsr;
    // T19-005: set when the template is an alias template. / Presente cuando la plantilla es una plantilla alias.
    std::optional<CppTemplateAliasTarget> aliasTarget;
    std::vector<std::string> diagnostics;
    // T13-007: the mirrors of NativeSharp structs this session declared. / Los espejos que declaro la sesion.
    std::vector<CppNativeStructLayout> nativeLayouts;
};

// EN: T14-001. A macro the header defines, as NativeSharp can use it: an object-like macro that expands to a
//     constant is imported as one; a function-like macro or any other expansion is not.
// ES: T14-001. Una macro que define el encabezado, segun la puede usar NativeSharp: una macro de objeto que se
//     expande a una constante se importa como tal; una macro de funcion u otra expansion no.
struct CppMacroName
{
    std::string name;
    bool functionLike = false;
    bool constant = false;
};

struct CppTemplateName
{
    std::string qualifiedName;
    bool function = false;
};

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
    // Experimental C-only query for an observed function-pointer typedef.
    // The plan's synthetic symbol is not a linkable function or an emission authorization.
    virtual std::optional<abi_plan::Plan> cFunctionPointerAbiPlan(std::string_view, std::string_view) const
    { return std::nullopt; }
    // C++ alias query: qualified name, USR and canonical pointer type must identify one declaration.
    // For regcall and sysv_abi; its synthetic symbol is not a linkable function.
    virtual std::optional<abi_plan::Plan> cppFunctionPointerAbiPlan(
        std::string_view, std::string_view, std::string_view) const
    { return std::nullopt; }
    // Types must come from this session and describe already-promoted optional
    // arguments. Identity and layout checks are not proof of cross-session ODR
    // equivalence. nullopt includes a closed session or unsupported form.
    virtual std::optional<abi_plan::VariadicCallPlan> cVariadicCallAbiPlan(
        std::string_view, std::string_view, const std::vector<CppType>&) const
    { return std::nullopt; }
    // Experimental C-only observation for one variadic function-pointer typedef.
    // Its synthetic symbol is not linkable and the result does not authorize emission.
    virtual std::optional<abi_plan::VariadicCallPlan> cVariadicFunctionPointerCallAbiPlan(
        std::string_view, std::string_view, const std::vector<CppType>&) const
    { return std::nullopt; }
    // Initial projection of named, non-dependent records; nullopt means closed.
    virtual std::optional<std::vector<CppRecordDeclaration>> recordsSnapshot() const = 0;
    // Live C AST observation only: true means a unique, complete, plain record
    // with unqualified builtin or named-record fields, fixed arrays or
    // attribute-free typedefs or named bitfields, and no declaration attributes. This observation
    // alone is not an ABI-equivalence proof.
    virtual std::optional<bool> cRecordHasPlainReplaySyntax(std::string_view) const { return std::nullopt; }
    // Plain, trivial, standard-layout C++ struct without bases or ABI-relevant attributes.
    virtual std::optional<bool> cppRecordHasPlainReplaySyntax(std::string_view) const { return std::nullopt; }
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
    // Live C++ Sema selection; nullopt means closed, ambiguous record name,
    // incomplete record or unavailable Sema. This does not authorize a call.
    virtual std::optional<CppRecordValueOperations> recordValueOperations(std::string_view) const
    { return std::nullopt; }
    // Experimental isolated IR only; no object, linkage, or lifetime integration is implied.
    virtual std::optional<std::string> emitImplicitLifetimeIR(
        std::string_view, CppImplicitLifetimeKind) const { return std::nullopt; }
    // Isolated C static-inline IR; the caller owns symbol uniqueness and linkage integration.
    virtual std::optional<std::string> emitCStaticInlineIR(
        const std::vector<CppCInlineIRExport>&,
        const std::vector<CppCInlineIRExport>& = {}) const { return std::nullopt; }
    // T10-012: IR with weak_odr definitions for the requested C++ symbols whose definition is inline in
    // this translation unit (functions and inline variables); `emitted` lists the symbols defined.
    // Symbols without an inline definition are left to the library that defines them.
    // T17-008: `debugInformation` asks Clang for CodeView line tables or full debug information in that IR.
    //     / `debugInformation` pide a Clang tablas de lineas CodeView o depuracion completa en ese IR.
    virtual std::optional<std::string> emitCppInlineIR(
        const std::vector<std::string>&, std::vector<std::string>& emitted,
        CppDebugInformation debugInformation = CppDebugInformation::None) const
    { emitted.clear(); (void)debugInformation; return std::nullopt; }
    // T12-004: IR modules with weak_odr definitions of the requested member adapters (cppMemberThunkSymbol)
    // for the records of this translation unit; `emitted` lists the adapters defined.
    virtual std::vector<std::string> emitCppMemberThunkIR(
        const std::vector<std::string>&, std::vector<std::string>& emitted) const
    { emitted.clear(); return {}; }
    // T13-006: the class and function templates declared at namespace scope.
    virtual std::vector<CppTemplateName> templatesSnapshot() const { return {}; }
    // T14-001: the macros of the header and whether each is an imported constant. / Las macros del encabezado.
    virtual std::vector<CppMacroName> macrosSnapshot() const { return {}; }
    // T13-010: Sema instantiates the template with the arguments; nullopt when it is not a template here.
    virtual std::optional<CppTemplateInstantiation> instantiateTemplate(
        std::string_view, bool, std::span<const CppTemplateArgument>) const { return std::nullopt; }
    // T13-005: Sema deduces the arguments of the function templates with that name from the types of a call's
    // arguments; nullopt when it declares no such template. Deduced specializations are imported under the
    // template's own name. / Sema deduce los argumentos de las plantillas de funcion con ese nombre a partir de
    // los tipos de los argumentos de una llamada; las especializaciones deducidas se importan con su nombre.
    virtual std::optional<CppTemplateInstantiation> deduceFunctionTemplateCall(
        std::string_view, std::span<const CppTemplateArgument>) const { return std::nullopt; }
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
    // EN: T19-003. The analyzed C/C++ source preprocessed by Clang with its macro definitions kept (`-E -dD`),
    //     with line markers, so a precompiled consumer can expand the function-like macros of a published template
    //     without the header; nullopt when it is not available.
    // ES: T19-003. La fuente C/C++ analizada, preprocesada por Clang conservando sus definiciones de macros
    //     (`-E -dD`), con marcas de linea, para que un consumidor precompilado expanda las macros de funcion de una
    //     plantilla publicada sin el encabezado; nullopt si no esta disponible.
    virtual std::optional<std::string> preprocessedSourceWithMacros() const { return std::nullopt; }
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
