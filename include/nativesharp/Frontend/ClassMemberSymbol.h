#pragma once

#include "nativesharp/Frontend/ClassMemberAccessibility.h"
#include "nativesharp/Frontend/ClassMemberKind.h"
#include "nativesharp/Frontend/InliningPolicy.h"
#include "nativesharp/Frontend/MethodParameterSymbol.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Language/OwnershipSemantics.h"
#include "nativesharp/Sema/EffectContext.h"
#include "nativesharp/Sema/OwnershipKind.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
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
    bool isTransportedInlineBody = false;
    // Exact physical identity published by the producer's NSMETA.  A precompiled callable is
    // resolved semantically against the synthetic declaration, but its external symbol must
    // never be regenerated from that declaration: source materialization can use a different
    // spelling for an otherwise identical closed generic type.
    std::string precompiledMangledName;
    std::string name;
    const ast::TypeSyntax* declaredTypeSyntax = nullptr;
    std::string declaredTypeKey;
    // NativeSharp 3.0 T07-F02. Empty means non-throwing. A non-empty key is part of the
    // callable type/effect contract while the physical Windows x64 return ABI stays unchanged.
    const ast::TypeSyntax* declaredErrorTypeSyntax = nullptr;
    std::string declaredErrorTypeKey;
    // NS304-T05-F08: async frame storage selected by [FrameStorage(...)] (default CallerStack).
    language::FrameStorage frameStorage = language::FrameStorage::CallerStack;
    bool declaredPointerPointeeReadonly = false;
    bool declaredPointerNullable = true;
    bool declaredPointerHasProvenance = true;
    std::size_t declaredPointerKnownAlignment = 1;
    unsigned declaredPointerAddressSpace = 0;
    sema::OwnershipKind declaredOwnership = sema::OwnershipKind::None;
    std::vector<MethodParameterSymbol> parameters;
    ast::FfiUnwindSyntaxKind ffiUnwindPolicy = ast::FfiUnwindSyntaxKind::Default;
    std::string signatureKey;
    // NativeSharp 3.0 T04-F06. Closed generic methods retain the template that produced them so
    // overload resolution can compare the declaration constraints after ordinary conversions.
    std::optional<std::size_t> genericTemplateOrdinal;
    bool isControlledSpecialization = false;
    bool isControlledSpecializationFamily = false;
    std::string genericTemplateKey;
    bool hasImplicitThis = false;
    bool declaringTypeIsStruct = false;
    bool isStatic = false;
    bool isNewSlot = false;
    bool isReadonly = false;
    // Item 313. declaredTypeKey remains the referent T; this bit makes the callable result an
    // lvalue and changes only its physical ABI carrier to T*.
    bool isReferenceReturn = false;
    std::string returnLifetimeName;
    std::vector<std::string> lifetimeParameters;
    std::vector<std::pair<std::string, std::string>> lifetimeOutlivesConstraints;
    // NativeSharp 3.0 T06-F07. One stable interprocedural escape contract per parameter.
    // Empty means that no producer contract was available and consumers must fail closed.
    std::vector<language::EscapeClass> parameterEscapeSummary;
    // Item 308. A volatile field is an observable memory location: every load/store survives
    // optimization, but it gains neither atomicity nor inter-thread ordering.
    bool isVolatile = false;
    bool isConst = false;
    // Item 327. Bit-field syntax retains the width expression without assigning physical bit
    // offsets yet. Unnamed fields use an unreachable compiler-generated identity until item 328
    // groups them into MSVC storage units.
    bool isBitField = false;
    bool isUnnamedBitField = false;
    const ast::AstNode* bitWidthSyntax = nullptr;
    InliningPolicy inliningPolicy = InliningPolicy::Automatic;
    // Item 321. The callable body may not contain an operation that allocates heap storage.
    bool isNoHeap = false;
    // T13-F05: a direct call requires an explicit unsafe block at the call site.
    bool isUnsafe = false;
    // NativeSharp 3.0 T00-F04. `isNoHeap` remains as an ABI-compatible projection while all
    // semantic consumers use this extensible set for noheap/throws/async and future effects.
    sema::EffectSet effects;
    bool isVirtual = false;
    bool isOverride = false;
    bool isAbstract = false;
    bool isSealed = false;
    bool isExported = false;
    bool isExternC = false;
    bool isExportedC = false;
    // Item 331. The declaration has one or more fixed C parameters followed by `...`.
    bool isVariadic = false;
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
    bool isInit = false;
    bool isRequired = false;
    bool isUserOperator = false;
    // NativeSharp 3.0 T05-F07. Stable semantic identity (for example `lang:Add`) shared by a
    // trait operator requirement and every concrete witness. The ABI method name remains in
    // `name`; overload resolution must not use that historical spelling as the language contract.
    std::string operatorTraitKey;
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
    // Item 326. A promoted field is a semantic alias owned by the containing type. It is not a
    // second physical field: the source member remains inside the anonymous aggregate and this
    // path names the compiler-generated carrier fields that lead from the containing object to it.
    bool isAnonymousMemberPromotion = false;
    std::optional<std::size_t> promotedSourceMemberOrdinal;
    std::vector<std::size_t> anonymousCarrierMemberOrdinals;
};
}
