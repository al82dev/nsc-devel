#pragma once

#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::ast
{
class AstNode;

enum class OwnershipQualifierSyntaxKind
{
    None,
    Owned,
    Borrowed
};

std::string_view ownershipQualifierSyntaxKindName(OwnershipQualifierSyntaxKind kind) noexcept;

enum class TypeModifierSyntaxKind
{
    Pointer,
    Array,
    DynamicArray,
    RangeArray,
    FlexibleArray
};

struct TypeModifierSyntax
{
    TypeModifierSyntaxKind kind = TypeModifierSyntaxKind::Pointer;
    AstNode* arrayCount = nullptr;
    SourceRange range;
    std::size_t arrayRank = 0;
};

class TypeSyntax final
{
public:
    TypeSyntax(SourceRange range, lexer::TokenKind baseTokenKind, std::string baseName);

    SourceRange range() const noexcept;
    void setRange(SourceRange range) noexcept;
    lexer::TokenKind baseTokenKind() const noexcept;
    const std::string& baseName() const noexcept;
    void setBaseName(std::string baseName);
    // C3 "FFI aliases". A `using` type alias is a TYPEDEF, not a distinct type: `using DWORD =
    // uint;` means DWORD IS uint, everywhere, with the same layout, the same ABI and the same
    // conversions. The cheapest way to make that true in every later stage at once is to expand
    // the alias INTO the type syntax at name-binding time -- after which the AST simply says
    // `uint`, and nothing downstream (canonical type keys, layout, HIR, the C ABI rules) needs to
    // know aliases exist at all. These two setters are what that expansion needs: replace the
    // base, and put the alias's own modifiers UNDER the ones written at the use site, so
    // `using PDWORD = uint*;` used as `PDWORD*` becomes `uint**` and not `uint*` twice over.
    void setBaseTokenKind(lexer::TokenKind kind) noexcept;
    void prependModifiers(const std::vector<TypeModifierSyntax>& modifiers);

    OwnershipQualifierSyntaxKind ownershipQualifier() const noexcept;
    void setOwnershipQualifier(OwnershipQualifierSyntaxKind qualifier) noexcept;

    // NativeSharp 3.0 source-level cost marker. `dyn Trait` intentionally shares the canonical
    // interface type/ABI, but retaining the spelling lets name lookup reject non-interface
    // targets and diagnostics/dumps keep dynamic dispatch explicit.
    bool isDynamicTrait() const noexcept;
    void setDynamicTrait(bool value = true) noexcept;

    // NativeSharp 3.0 T05-F04. `some Trait` is a declaration-scoped opaque result, not a
    // dynamic trait object. The parser keeps the requested trait here until the early frontend
    // replaces the source return with its generated nominal value wrapper.
    bool isOpaqueResult() const noexcept;
    void setOpaqueResult(bool value = true) noexcept;
    const std::string& opaqueResultTraitName() const noexcept;
    void setOpaqueResultTraitName(std::string name);

    // NativeSharp 3.0 T05-F02. Marks a base-list entry introduced by
    // `impl Trait for Type` rather than by the type declaration itself. The resolved type is
    // deliberately unchanged: this bit only preserves provenance so coherence can reject a
    // duplicate impl deterministically after name binding, including on generic clones.
    bool isExplicitTraitImplementation() const noexcept;
    void setExplicitTraitImplementation(bool value = true) noexcept;

    // NativeSharp 3.0 T42: `T const*` qualifies the pointed-to T. This is deliberately
    // independent from a declaration's leading `const`, which qualifies the binding itself
    // (`const T* p`). The bit is semantic-only: it does not alter the canonical Win64 ABI type.
    bool isPointeeConst() const noexcept;
    void setPointeeConst(bool value = true) noexcept;

    void addTypeArgument(TypeSyntax* typeArgument);
    const std::vector<TypeSyntax*>& typeArguments() const noexcept;
    void clearTypeArguments() noexcept;

    // NativeSharp 3.0 structural tuple type. Tuple element names are source metadata only: the
    // canonical identity is derived exclusively from typeArguments(), so `(int x, long y)` and
    // `(int a, long b)` are the same type. Keeping the names here still lets diagnostics and the
    // AST dumper reproduce what the programmer wrote.
    bool isTuple() const noexcept;
    void setTuple(bool value = true) noexcept;
    void addTupleElement(TypeSyntax* type, std::string name = {});
    const std::vector<std::string>& tupleElementNames() const noexcept;

    // NativeSharp 3 const-generic argument. The parser temporarily keeps the ordinary expression
    // AST here; ProjectGenericModel evaluates it deterministically and replaces this carrier with
    // the canonical const marker before specialization identity or lowering observes the type.
    AstNode* constExpression() const noexcept;
    void setConstExpression(AstNode* expression) noexcept;

    bool isPackExpansion() const noexcept;
    void setPackExpansion(bool value = true) noexcept;

    void addModifier(TypeModifierSyntax modifier);
    const std::vector<TypeModifierSyntax>& modifiers() const noexcept;

    // Stage 52 fix: normally a TypeSyntax node is bound (ProjectNameLookup::bindTypeSyntax)
    // using the project ordinal of whichever ProjectSyntaxUnit's tree it lexically lives in.
    // That breaks for a generic type ARGUMENT that gets substituted into a template's cloned
    // specialization body (ProjectGenericModel::cloneType/cloneNode): the clone is attached to
    // and walked as part of the TEMPLATE's own defining project/unit (e.g. a shared library),
    // not the REQUESTING project that actually wrote the argument (e.g. an application project
    // with a <ProjectReference> to that library) -- so an argument type declared only in the
    // requesting project (the overwhelmingly common case: "List<MyOwnClass>") was reported as
    // an "unknown type name", because the library's own project cannot see it and the library
    // never depends on its consumers. This field lets a substituted node carry the argument
    // DECLARATION project's ordinal explicitly (or the requesting project as a conservative
    // fallback when the early generic pass cannot select the declaration), overriding the
    // enclosing template unit. Unset (nullopt) for every ordinary, non-substituted type syntax
    // node -- fully backward compatible.
    std::optional<std::size_t> originProjectOrdinal() const noexcept;
    void setOriginProjectOrdinal(std::size_t projectOrdinal) noexcept;

    // Stage 52 fix: companion to originProjectOrdinal, needed for the SAME cross-project
    // generics scenario -- but for the lexical namespace rather than the project. A substituted
    // node is walked (ProjectNameLookup::bindTypeSyntax) under the TEMPLATE's own namespace
    // (e.g. "NativeSharp.Core.Collections.Generic", from List<T>'s body), not the argument
    // declaration's namespace (which may belong to a third referenced project) -- so an
    // unqualified argument type name needs this override to be found via ordinary lookup.
    // Deliberately kept SEPARATE from baseName(): an earlier version of this fix prefixed baseName()
    // itself with the requesting namespace, which broke generic specialization identity --
    // ProjectGenericModel's specialization cache key is derived from typeSpelling(), i.e. the
    // argument's raw baseName(), so "Predicate<Payload>" (from the application's own top-level
    // declaration, argument written and left exactly as "Payload") and "Predicate<Payload>" (from
    // List<Payload>'s internal nested Predicate<T> use, argument being this SAME already-tagged
    // "Payload" node reused via substitution) must spell IDENTICALLY to specialize to the same
    // type -- mutating baseName() made the nested case spell "Predicate<Stage52LibTest.Payload>"
    // instead, silently producing two distinct, incompatible specializations of what is
    // semantically one closed generic type, and breaking overload resolution between them.
    // Resolution now goes entirely through this dedicated override (read only by
    // ProjectNameLookup::bindTypeSyntax), leaving baseName() -- and therefore every specialization
    // key derived from it -- untouched.
    std::optional<std::string> originNamespace() const noexcept;
    void setOriginNamespace(std::string namespaceName) noexcept;

private:
    SourceRange range_;
    lexer::TokenKind baseTokenKind_;
    std::string baseName_;
    OwnershipQualifierSyntaxKind ownershipQualifier_ = OwnershipQualifierSyntaxKind::None;
    bool isDynamicTrait_ = false;
    bool isOpaqueResult_ = false;
    std::string opaqueResultTraitName_;
    bool isExplicitTraitImplementation_ = false;
    bool isPointeeConst_ = false;
    std::vector<TypeSyntax*> typeArguments_;
    bool isTuple_ = false;
    std::vector<std::string> tupleElementNames_;
    AstNode* constExpression_ = nullptr;
    bool isPackExpansion_ = false;
    std::vector<TypeModifierSyntax> modifiers_;
    std::optional<std::size_t> originProjectOrdinal_;
    std::optional<std::string> originNamespace_;
};
}
