#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"
#include "nativesharp/Frontend/InliningPolicy.h"
#include "nativesharp/Sema/OwnershipKind.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
enum class ClassMemberKind
{
    Field,
    Event,
    Method,
    Constructor,
    Destructor
};

enum class ClassMemberAccessibility
{
    Private,
    Protected,
    Internal,
    Public
};

// C4 "language references". How a parameter receives its argument.
//
// All three by-reference forms are ONE machine-level thing -- a pointer -- and that is deliberate:
// C1 already made `&x`, `*p` and pointer parameters work end to end, so `ref`/`out`/`in` are
// sugar over machinery that is already certified rather than a new calling convention. What
// distinguishes them is entirely a matter of RULES the frontend enforces:
//
//   Ref -- the argument must already hold a value, and the callee may read and write it.
//   Out -- the callee MUST assign it before returning; the caller need not initialize it first.
//   In  -- pass by reference for cost, read-only at the callee. No copy, no writes.
//
// The keyword is required at the CALL SITE too, exactly as in C#, so that a reader of the call
// can see that the argument may change without having to go read the signature.
enum class ParameterPassKind
{
    Value,
    Ref,
    Out,
    In
};

const char* parameterPassKindName(ParameterPassKind kind) noexcept;
const char* parameterPassKindKeyword(ParameterPassKind kind) noexcept;

struct MethodParameterSymbol final
{
    std::string name;
    // C4: Value for every ordinary parameter. For the other three, `typeKey` is the POINTER type
    // (`T*`) -- the parser adds that modifier -- because a by-reference parameter genuinely
    // receives an address; `pointeeTypeKey` is what the programmer wrote and what a use of the
    // parameter inside the body evaluates to.
    ParameterPassKind passKind = ParameterPassKind::Value;
    std::string pointeeTypeKey;
    const ast::TypeSyntax* typeSyntax = nullptr;
    std::string typeKey;
    sema::OwnershipKind ownership = sema::OwnershipKind::None;
    FileId fileId;
    SourceRange range;
    const ast::AstNode* syntax = nullptr;
    // NSC-LIMIT-020. Defaults remain attached to the declaration AST so ProjectReference calls
    // materialize the value fixed by the callee's contract, not a caller-side reparse.
    const ast::AstNode* defaultValueSyntax = nullptr;
};

struct ClassMemberSymbol final
{
    ClassMemberKind kind = ClassMemberKind::Field;
    ClassMemberAccessibility accessibility = ClassMemberAccessibility::Private;
    std::size_t declaringTypeOrdinal = 0;
    std::size_t partOrdinal = 0;
    std::size_t sourceOrdinal = 0;
    std::size_t declarationOrdinal = 0;
    FileId fileId;
    SourceRange range;
    const ast::AstNode* syntax = nullptr;
    std::string sourcePath;
    // The origin belongs to the declaration PART, not necessarily to the complete partial type.
    // A metadata API part may be precompiled while a transported generic-template part is local
    // and must therefore be analyzed/lowered in the consumer.
    bool isPrecompiled = false;
    // Exact physical identity published by the producer's NSMETA.  A precompiled callable is
    // resolved semantically against the synthetic declaration, but its external symbol must
    // never be regenerated from that declaration: source materialization can use a different
    // spelling for an otherwise identical closed generic type.
    std::string precompiledMangledName;
    std::string name;
    const ast::TypeSyntax* declaredTypeSyntax = nullptr;
    std::string declaredTypeKey;
    sema::OwnershipKind declaredOwnership = sema::OwnershipKind::None;
    std::vector<MethodParameterSymbol> parameters;
    std::string signatureKey;
    bool hasImplicitThis = false;
    bool declaringTypeIsStruct = false;
    bool isStatic = false;
    bool isNewSlot = false;
    bool isReadonly = false;
    // Item 308. A volatile field is an observable memory location: every load/store survives
    // optimization, but it gains neither atomicity nor inter-thread ordering.
    bool isVolatile = false;
    bool isConst = false;
    InliningPolicy inliningPolicy = InliningPolicy::Automatic;
    bool isVirtual = false;
    bool isOverride = false;
    bool isAbstract = false;
    bool isSealed = false;
    bool isExported = false;
    bool isExternC = false;
    bool isExportedC = false;
    // Interop Foundation: true for a function declared inside `extern C from "<dll>" { ... }` --
    // a body-less static member whose C symbol is `name` and whose import library is derived
    // from `importDllName` at link time. The compiler knows no API by name; only this shape.
    bool isImportedC = false;
    std::string importDllName;
    // C1.2 "import identity": set when this import's C symbol name is claimed by MORE THAN ONE
    // library in the project (`a.dll!Init` and `b.dll!Init`). A PE import library keys an import
    // by its symbol NAME, so one name cannot be resolved statically against two DLLs -- this
    // import binds at first call through LoadLibrary/GetProcAddress instead, and is deliberately
    // left OUT of the import-library plan. False for every import whose name only one library
    // claims, which leaves the certified static-by-name path exactly as it was.
    bool importRequiresRuntimeBinding = false;
    std::string cAbiName;
    bool isCompilerGenerated = false;
    bool isEvent = false;
    bool isEventBacking = false;
    bool isPropertyAccessor = false;
    bool isIndexerAccessor = false;
    bool isGetter = false;
    bool isSetter = false;
    bool isUserOperator = false;
    bool isUserConversion = false;
    bool isImplicitUserConversion = false;
    // Bloque 1 item 9 (ownership-aware properties/indexers). Set on BOTH accessors of a
    // property or indexer whose declared type carries the `owned` qualifier.
    //
    // What `owned` means on a property/indexer is deliberately asymmetric, and the asymmetry is
    // the whole design decision:
    //
    //   * The SETTER takes ownership. Its implicit `value` parameter is `owned`, so
    //     `h.Slot = move(a)` is a real transfer and the receiver becomes responsible for the
    //     object. That is the half the feature exists for.
    //
    //   * The GETTER hands out a NON-OWNING PLAIN REFERENCE, never ownership. Its
    //     `declaredOwnership` is reset to None right where this flag is set, so a getter body may
    //     `return backing;` and a caller that writes `Res r = h.Slot;` gets a plain alias.
    //
    //     PRECISION (audit finding, terminologia): this is deliberately NOT a formal `borrowed`.
    //     A borrowed value in this language carries a PROVEN lifetime binding back to a live
    //     owner, and a field read has no such provenance to offer (measured: a borrowed-declared
    //     getter is rejected with "borrowed return requires a borrowed value whose lifetime
    //     outlives the call"). So the alias belongs to the language's THIRD reference category --
    //     plain/raw, non-owning, lifetime-unchecked, exactly like every read-only container
    //     accessor already returns. An alias that outlives its container dangles BY DESIGN, the
    //     way a C++ raw reference would; documentation must never present it as borrow safety.
    //
    // A getter cannot transfer ownership out, and that is not a lowering limitation -- it is
    // unwritable for a sound reason. If reading the property gave the caller ownership, the
    // object would still be holding the same pointer in an `owned` field that its own
    // destruction entry releases: two owners, one allocation, guaranteed double free. The only
    // alternative -- clearing the field on read -- would make a read mutate the object, which is
    // not what a getter is. So `owned` here describes WHERE THE STORAGE LIVES (this object owns
    // it), not what every accessor does with it.
    //
    // This is exactly the shape the library had already settled on by hand while the qualifier
    // was undeclarable: `List<T>` exposes a read-only indexer returning a plain `T` and writes
    // through `SetAt(int, owned T)`. Item 9 makes that shape expressible as one declaration
    // instead of two members.
    bool ownsAccessorStorage = false;
    std::string associatedName;
    // Stage 51.x: enum member support in the OOP pipeline. An enum constant is registered as
    // an ordinary `static const int` field (reusing the Stage 37 native constant-field lowering
    // end to end) plus this precomputed value, since enum member declarations have no real
    // initializer expression to fold for the (common) auto-increment case.
    bool isEnumConstant = false;
    std::int64_t enumConstantValue = 0;
    // C2.2 "union and explicit layout": the byte offset this field names for itself with
    // `at(N)`, if it named one. Unset means "wherever the layout rule puts it", which is the
    // normal case for every field in the language.
    std::optional<std::uint64_t> explicitFieldOffset;
    // `align(N)` on an instance field raises that field's placement alignment and therefore the
    // containing aggregate's alignment. It is a minimum, never a request to weaken natural layout.
    std::optional<std::uint64_t> explicitFieldAlignment;
};

struct PrecompiledMemberIdentity final
{
    ClassMemberKind kind = ClassMemberKind::Field;
    std::string declaringTypeKey;
    std::string name;
    std::string signatureKey;
    std::string mangledName;
};

enum class MemberBodyBindingKind
{
    ThisReference,
    ThisMemberAccess
};

struct MemberBodyBinding final
{
    MemberBodyBindingKind kind = MemberBodyBindingKind::ThisReference;
    const ast::AstNode* syntax = nullptr;
    std::size_t declaringTypeOrdinal = 0;
    std::size_t methodMemberOrdinal = 0;
    std::string memberName;
    std::vector<std::size_t> candidateMemberOrdinals;
};

class ProjectMemberModel final
{
public:
    bool build(
        const ProjectNameLookup& nameLookup,
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics);
    bool bindPrecompiledPhysicalNames(
        std::span<const PrecompiledMemberIdentity> identities,
        std::span<const std::pair<std::string, std::string>> typeKeyAliases,
        std::string& error);

    std::span<const ClassMemberSymbol> members() const noexcept;
    std::span<const MemberBodyBinding> bodyBindings() const noexcept;
    std::span<const std::size_t> membersOfType(std::size_t typeOrdinal) const noexcept;
    std::vector<std::size_t> findMembers(
        std::size_t typeOrdinal,
        std::string_view name) const;
    bool isBuilt() const noexcept;
    std::string canonicalTypeKey(
        const ast::TypeSyntax* syntax,
        const ProjectNameLookup& nameLookup) const;

    // C2.1 "fixed arrays". The compile-time value of a `T[N]` count expression, or nullopt when
    // it is not a constant. It lives here, next to canonicalTypeKey, because the CANONICAL TYPE
    // KEY is the first consumer: a fixed-array key must read `builtin:float[4]`, with the count
    // itself in it, so that every later stage (element type, element size, ABI classification)
    // can answer questions about the type from the key alone. Before this, the key carried an
    // opaque fingerprint of the count's SYNTAX (`builtin:float[38:4:3:4]`), which nothing
    // downstream could interpret -- which is why indexing such a field never lowered.
    // ProjectObjectLayoutModel delegates to this so there is exactly one definition of "what
    // counts as a constant array count" in the compiler.
    static std::optional<std::uint64_t> evaluateConstantArrayCount(const ast::AstNode* expression) noexcept;

    // C2.1: the element type key of a fixed-array key ("builtin:float[4]" -> "builtin:float"),
    // and its count. Both return nullopt for anything that is not a fixed array -- a dynamic
    // array ("T[]"), a pointer, or a plain type.
    static std::optional<std::string> fixedArrayElementTypeKey(std::string_view typeKey);
    static std::optional<std::uint64_t> fixedArrayCount(std::string_view typeKey);
    static std::optional<std::string> flexibleArrayElementTypeKey(std::string_view typeKey);

private:
    std::string syntaxFingerprint(const ast::AstNode* node) const;
    bool collectTypeMembers(
        std::size_t typeOrdinal,
        const NamedTypeDeclaration& type,
        const ProjectNameLookup& nameLookup,
        diagnostics::DiagnosticEngine& diagnostics);
    bool collectEnumMembers(
        std::size_t typeOrdinal,
        const NamedTypeDeclaration& type,
        diagnostics::DiagnosticEngine& diagnostics);
    void scanMethodBody(
        const ast::AstNode* node,
        std::size_t typeOrdinal,
        std::size_t methodMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void scanInvalidThisContexts(
        const ast::AstNode* node,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void reportDuplicate(
        const ClassMemberSymbol& current,
        const ClassMemberSymbol& previous,
        diagnostics::DiagnosticEngine& diagnostics,
        std::string message) const;

    const ProjectNameLookup* nameLookup_ = nullptr;
    std::vector<ClassMemberSymbol> members_;
    std::vector<MemberBodyBinding> bodyBindings_;
    std::vector<std::vector<std::size_t>> membersByType_;
    std::unordered_map<const ast::AstNode*, std::size_t> memberBySyntax_;
    bool built_ = false;
};

const char* classMemberKindName(ClassMemberKind kind) noexcept;
const char* classMemberAccessibilityName(ClassMemberAccessibility accessibility) noexcept;
const char* memberBodyBindingKindName(MemberBodyBindingKind kind) noexcept;
}
