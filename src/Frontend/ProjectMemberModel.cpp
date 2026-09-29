#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Language/ExpressionErgonomics.h"

#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
namespace
{
sema::OwnershipKind ownershipKindFromSyntax(const ast::TypeSyntax* syntax) noexcept
{
    if (syntax == nullptr)
    {
        return sema::OwnershipKind::None;
    }

    switch (syntax->ownershipQualifier())
    {
        case ast::OwnershipQualifierSyntaxKind::None: return sema::OwnershipKind::None;
        case ast::OwnershipQualifierSyntaxKind::Owned: return sema::OwnershipKind::Owned;
        case ast::OwnershipQualifierSyntaxKind::Borrowed: return sema::OwnershipKind::Borrowed;
    }
    return sema::OwnershipKind::None;
}

std::string ownershipSignaturePrefix(sema::OwnershipKind ownership)
{
    switch (ownership)
    {
        case sema::OwnershipKind::None: return {};
        case sema::OwnershipKind::Owned: return "owned ";
        case sema::OwnershipKind::Borrowed: return "borrowed ";
    }
    return {};
}

const NamedTypeDeclaration* namedTypeForKey(
    std::string_view typeKey,
    const ProjectNameLookup& nameLookup) noexcept
{
    if (!typeKey.starts_with("type:")) return nullptr;
    const std::string_view key = typeKey.substr(5);
    for (const NamedTypeDeclaration& type : nameLookup.namedTypes())
    {
        if (type.identity.key() == key) return &type;
    }
    return nullptr;
}

// SHR-013 "vistas y tiempo de vida": el tipo se declaro `borrowed struct`. Lee la MISMA marca
// que el parser escribio en el texto auxiliar de la declaracion y que ProjectMethodBodyModel
// vuelve a leer -- una marca, dos lectores, cero copias de la regla.
bool namedTypeIsBorrowedView(const NamedTypeDeclaration* type) noexcept
{
    return type != nullptr && type->syntax != nullptr &&
        type->syntax->auxiliaryText().find("borrowedview") != std::string::npos;
}

bool typeKeyIsViewLike(std::string_view typeKey, const ProjectNameLookup& nameLookup) noexcept
{
    std::string_view key = typeKey;
    while (key.ends_with("[]")) key = key.substr(0, key.size() - 2);
    return namedTypeIsBorrowedView(namedTypeForKey(key, nameLookup));
}

bool isReservedStage47CAbiExportName(std::string_view name) noexcept
{
    if (name.starts_with("__nativesharp_") || name.starts_with("__nsc_") ||
        name.starts_with("llvm."))
    {
        return true;
    }
    constexpr std::string_view reserved[] = {
        "ExitProcess", "GetProcessHeap", "HeapAlloc", "HeapFree", "GetStdHandle",
        "WriteFile", "TlsAlloc", "TlsGetValue", "TlsSetValue", "RaiseException",
        "__chkstk", "__C_specific_handler", "strlen", "memset", "memcpy", "memmove",
        "puts", "_fltused"};
    return std::find(std::begin(reserved), std::end(reserved), name) != std::end(reserved);
}

// C1.2 "import identity": DLL names compare case-insensitively ("user32.dll" == "USER32.DLL"),
// exactly as the linker's import-library planner already merges them.
std::string upperAsciiText(std::string_view text)
{
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

bool isStage47CAbiType(
    std::string_view typeKey,
    const ProjectNameLookup& nameLookup,
    bool allowVoid) noexcept
{
    if (typeKey == "builtin:void") return allowVoid;
    // ID 89. `sbyte` en la lista de tipos que cruzan la ABI de C, y esta es LA razon por la que
    // el tipo existe: `signed char` es un parametro corriente en las cabeceras de C, y sin
    // `sbyte` no habia forma de declararlo honestamente -- habia que mentir con `byte` y dejar
    // que el signo se perdiera en la frontera. Si el tipo estuviera en todas las tablas de ancho
    // y signo pero no aqui, `extern C void f(sbyte x);` se rechazaria y el tipo no serviria para
    // lo unico que se pidio.
    if (typeKey == "builtin:bool" || typeKey == "builtin:sbyte" ||
        typeKey == "builtin:byte" || typeKey == "builtin:char" ||
        typeKey == "builtin:short" || typeKey == "builtin:ushort" ||
        typeKey == "builtin:int" || typeKey == "builtin:uint" ||
        typeKey == "builtin:long" || typeKey == "builtin:ulong" ||
        typeKey == "builtin:nint" || typeKey == "builtin:nuint" ||
        typeKey == "builtin:float" || typeKey == "builtin:double")
    {
        return true;
    }
    if (typeKey == "builtin:string" || typeKey.ends_with("[]") || typeKey.find('[') != std::string_view::npos)
    {
        return false;
    }
    if (!typeKey.empty() && typeKey.back() == '*')
    {
        return true;
    }
    const NamedTypeDeclaration* named = namedTypeForKey(typeKey, nameLookup);
    if (named == nullptr) return false;
    if (named->kind == NamedTypeDeclarationKind::Enum || named->kind == NamedTypeDeclarationKind::Struct ||
        named->kind == NamedTypeDeclarationKind::Class || named->kind == NamedTypeDeclarationKind::Interface)
    {
        // Classes/interfaces cross the host ABI only as opaque pointer-sized handles.
        return true;
    }
    if (named->kind == NamedTypeDeclarationKind::Delegate)
    {
        return named->syntax != nullptr && named->syntax->hasModifier(lexer::TokenKind::KwExtern);
    }
    return false;
}

// Item 308. MMIO accesses are intentionally limited to one native scalar transaction. Aggregates,
// managed arrays, strings, and object/delegate references would make "one volatile access"
// ambiguous because their value copy or lifetime machinery may require several memory operations.
bool isVolatileFieldType(
    std::string_view typeKey,
    const ProjectNameLookup& nameLookup) noexcept
{
    if (typeKey == "builtin:bool" || typeKey == "builtin:sbyte" ||
        typeKey == "builtin:byte" || typeKey == "builtin:char" ||
        typeKey == "builtin:short" || typeKey == "builtin:ushort" ||
        typeKey == "builtin:int" || typeKey == "builtin:uint" ||
        typeKey == "builtin:long" || typeKey == "builtin:ulong" ||
        typeKey == "builtin:nint" || typeKey == "builtin:nuint" ||
        typeKey == "builtin:float" || typeKey == "builtin:double" ||
        (!typeKey.empty() && typeKey.back() == '*'))
    {
        return true;
    }
    const NamedTypeDeclaration* named = namedTypeForKey(typeKey, nameLookup);
    return named != nullptr && named->kind == NamedTypeDeclarationKind::Enum;
}

ClassMemberAccessibility memberAccessibility(const ast::AstNode* node) noexcept
{
    if (node == nullptr)
    {
        return ClassMemberAccessibility::Private;
    }

    switch (node->tokenKind())
    {
        case lexer::TokenKind::KwPublic:
            return ClassMemberAccessibility::Public;
        case lexer::TokenKind::KwInternal:
            return ClassMemberAccessibility::Internal;
        case lexer::TokenKind::KwProtected:
            return ClassMemberAccessibility::Protected;
        case lexer::TokenKind::KwPrivate:
        case lexer::TokenKind::Invalid:
            return ClassMemberAccessibility::Private;
        default:
            return ClassMemberAccessibility::Private;
    }
}

bool isClassMemberNode(ast::AstNodeKind kind) noexcept
{
    return kind == ast::AstNodeKind::FieldDeclaration ||
        kind == ast::AstNodeKind::EventDeclaration ||
        kind == ast::AstNodeKind::MethodDeclaration ||
        kind == ast::AstNodeKind::ConstructorDeclaration ||
        kind == ast::AstNodeKind::DestructorDeclaration;
}

void appendExpandedMemberNodes(
    const ast::AstNode* node,
    std::vector<const ast::AstNode*>& output)
{
    if (node == nullptr)
    {
        return;
    }
    if (!node->genericParameters().empty())
    {
        return;
    }
    // Items 324-325. The direct node is the compiler-generated nested layout type. Its first edge is
    // the field that belongs to the containing type; children are the fields of the anonymous
    // aggregate itself and are collected only when that synthesized type gets its own member pass.
    if (node->kind() == ast::AstNodeKind::StructDeclaration &&
        node->auxiliaryText().starts_with("anonymous-") &&
        node->first() != nullptr && node->first()->kind() == ast::AstNodeKind::FieldDeclaration)
    {
        output.push_back(node->first());
        return;
    }
    // NSC-LIMIT-009. UN TIPO ANIDADO NO ES UN MIEMBRO EJECUTABLE, y se dice aqui en vez de
    // dejar que caiga por el final de la funcion sin coincidir con ninguna rama.
    //
    // La diferencia entre las dos cosas es que la segunda es un accidente: bastaria que alguien
    // anadiera una rama nueva para que un `struct` anidado empezara a registrarse como campo, y
    // el sintoma seria un miembro fantasma en el layout, no un error. `ProjectNameLookup` ya lo
    // registro como TIPO --con su identidad, su contenedor y su accesibilidad-- y esa es la
    // unica tabla en la que un tipo anidado tiene que aparecer.
    if (node->kind() == ast::AstNodeKind::ClassDeclaration ||
        node->kind() == ast::AstNodeKind::StructDeclaration ||
        node->kind() == ast::AstNodeKind::InterfaceDeclaration ||
        node->kind() == ast::AstNodeKind::EnumDeclaration ||
        node->kind() == ast::AstNodeKind::DelegateDeclaration)
    {
        return;
    }
    if (isClassMemberNode(node->kind()))
    {
        output.push_back(node);
        if (node->kind() == ast::AstNodeKind::EventDeclaration)
        {
            for (const ast::AstNode* child : node->children())
            {
                appendExpandedMemberNodes(child, output);
            }
        }
        return;
    }
    // Interop Foundation: an `extern C from "<dll>"` member block is a CONTAINER -- each child
    // is an imported-C-function MethodDeclaration (tagged "cimport:<dll>" by the parser) that
    // joins the class as an ordinary body-less static member.
    if (node->kind() == ast::AstNodeKind::ExternCDeclaration)
    {
        for (const ast::AstNode* child : node->children())
        {
            appendExpandedMemberNodes(child, output);
        }
        return;
    }
    if (node->kind() == ast::AstNodeKind::PropertyDeclaration ||
        node->kind() == ast::AstNodeKind::IndexerDeclaration)
    {
        for (const ast::AstNode* child : node->children())
        {
            appendExpandedMemberNodes(child, output);
        }
    }
}

bool isInvalidVoidObjectType(const ast::TypeSyntax* type) noexcept
{
    if (type == nullptr || type->baseTokenKind() != lexer::TokenKind::KwVoid)
    {
        return false;
    }

    const auto& modifiers = type->modifiers();
    return modifiers.empty() ||
        modifiers.front().kind == ast::TypeModifierSyntaxKind::Array ||
        modifiers.front().kind == ast::TypeModifierSyntaxKind::DynamicArray ||
        modifiers.front().kind == ast::TypeModifierSyntaxKind::FlexibleArray;
}
}

bool ProjectMemberModel::build(
    const ProjectNameLookup& nameLookup,
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (built_)
    {
        return !diagnostics.hasErrors();
    }

    nameLookup_ = &nameLookup;
    membersByType_.resize(nameLookup.namedTypes().size());
    promotedMembersByType_.resize(nameLookup.namedTypes().size());
    membersByName_.resize(nameLookup.namedTypes().size());
    typeOrdinalByKey_.reserve(nameLookup.namedTypes().size());
    for (std::size_t typeOrdinal = 0; typeOrdinal < nameLookup.namedTypes().size(); ++typeOrdinal)
    {
        typeOrdinalByKey_.emplace(nameLookup.namedTypes()[typeOrdinal].identity.key(), typeOrdinal);
    }

    for (std::size_t typeOrdinal = 0; typeOrdinal < nameLookup.namedTypes().size(); ++typeOrdinal)
    {
        const NamedTypeDeclaration& type = nameLookup.namedTypes()[typeOrdinal];
        if (type.kind == NamedTypeDeclarationKind::Delegate)
        {
            if (type.syntax != nullptr)
            {
                std::unordered_map<std::string, const ast::AstNode*> parameterNames;
                for (const ast::AstNode* parameter : type.syntax->children())
                {
                    if (parameter == nullptr || parameter->kind() != ast::AstNodeKind::ParameterDeclaration)
                    {
                        continue;
                    }
                    if (isInvalidVoidObjectType(parameter->typeSyntax()))
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidType,
                            diagnostics::DiagnosticSeverity::Error,
                            parameter->range(),
                            "delegate parameter '" + parameter->text() +
                                "' cannot have object type void");
                    }
                    const auto previous = parameterNames.find(parameter->text());
                    if (previous != parameterNames.end())
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::DuplicateSymbol,
                            diagnostics::DiagnosticSeverity::Error,
                            parameter->range(),
                            "duplicate delegate parameter '" + parameter->text() + "'");
                        diagnostics.report(
                            diagnostics::DiagnosticId::PreviousDeclaration,
                            diagnostics::DiagnosticSeverity::Note,
                            previous->second->range(),
                            "previous delegate parameter declaration is here");
                    }
                    else
                    {
                        parameterNames.emplace(parameter->text(), parameter);
                    }
                }
            }
            if (type.syntax != nullptr && type.syntax->hasModifier(lexer::TokenKind::KwExtern))
            {
                if (!type.syntax->genericParameters().empty())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidExternCSignature,
                        diagnostics::DiagnosticSeverity::Error,
                        type.range,
                        "extern C delegate types cannot be generic in Stage 47");
                }
                const std::string returnType = canonicalTypeKey(type.syntax->typeSyntax(), nameLookup);
                if (!isStage47CAbiType(returnType, nameLookup, true))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidExternCSignature,
                        diagnostics::DiagnosticSeverity::Error,
                        type.range,
                        "extern C delegate return type '" + returnType + "' is not C ABI compatible");
                }
                for (const ast::AstNode* parameter : type.syntax->children())
                {
                    if (parameter == nullptr || parameter->kind() != ast::AstNodeKind::ParameterDeclaration) continue;
                    const std::string parameterType = canonicalTypeKey(parameter->typeSyntax(), nameLookup);
                    if (!isStage47CAbiType(parameterType, nameLookup, false))
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidExternCSignature,
                            diagnostics::DiagnosticSeverity::Error,
                            parameter->range(),
                            "extern C delegate parameter type '" + parameterType + "' is not C ABI compatible");
                    }
                }
            }
            continue;
        }
        if (type.kind == NamedTypeDeclarationKind::Enum)
        {
            collectEnumMembers(typeOrdinal, type, diagnostics);
            continue;
        }
        if (type.kind != NamedTypeDeclarationKind::Class &&
            type.kind != NamedTypeDeclarationKind::Interface &&
            type.kind != NamedTypeDeclarationKind::Struct)
        {
            continue;
        }
        collectTypeMembers(typeOrdinal, type, nameLookup, diagnostics);
    }

    // Raw unions carry no active-member discriminant.  It is therefore unsafe for any
    // alternative to own storage, including ownership hidden inside an inline struct or fixed
    // array of structs: writing through that nested owner would run release-before-store against
    // whichever overlapping alternative supplied the current bits.  Validate the completed type
    // graph here, after every member has been collected, so declaration order, anonymous
    // aggregates and closed generic specializations cannot bypass the rule.
    for (std::size_t typeOrdinal = 0; typeOrdinal < nameLookup.namedTypes().size(); ++typeOrdinal)
    {
        const NamedTypeDeclaration& type = nameLookup.namedTypes()[typeOrdinal];
        const bool isRawUnion = type.kind == NamedTypeDeclarationKind::Struct &&
            type.syntax != nullptr &&
            type.syntax->auxiliaryText().find("union") != std::string::npos;
        if (!isRawUnion) continue;
        for (const std::size_t memberOrdinal : membersByType_[typeOrdinal])
        {
            if (memberOrdinal >= members_.size()) continue;
            const ClassMemberSymbol& member = members_[memberOrdinal];
            if (member.kind != ClassMemberKind::Field || member.isStatic) continue;
            if (member.declaredOwnership == sema::OwnershipKind::Owned)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    member.range,
                    "owned field '" + member.name +
                        "' is not permitted in a raw union because the union has no discriminant "
                        "that identifies the active member");
            }
            else if (typeTransitivelyOwnsStorage(member.declaredTypeKey))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    member.range,
                    "field '" + member.name + "' of type '" + member.declaredTypeKey +
                        "' is not permitted in a raw union because it transitively contains owned "
                        "storage and the union has no discriminant that identifies the active member");
            }
        }
    }

    collectAnonymousMemberPromotions(nameLookup, diagnostics);
    buildMemberNameIndex();

    std::unordered_map<std::string, const ClassMemberSymbol*> cAbiExports;
    for (const ClassMemberSymbol& member : members_)
    {
        if (!member.isExportedC) continue;
        const auto [iterator, inserted] = cAbiExports.emplace(member.cAbiName, &member);
        if (!inserted)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                member.range,
                "duplicate Stage 47 C ABI export name '" + member.cAbiName + "'");
            diagnostics.report(
                diagnostics::DiagnosticId::PreviousDeclaration,
                diagnostics::DiagnosticSeverity::Note,
                iterator->second->range,
                "previous C ABI export declaration is here");
        }
    }

    // C1.2 "import identity". The identity of an imported C function is the TRIPLE
    //
    //     (library, external symbol, signature)
    //
    // never the symbol name alone. Before this pass the compiler keyed imports by symbol name
    // only, and the omission was not academic: declaring `a.dll!Init` and `b.dll!Init` in one
    // project produced TWO LLVM declarations asking for the same name, LLVM silently renamed the
    // second to `Init.1`, and the link failed with "undefined symbol: Init.1" (measured). The
    // triple is enforced here in two halves:
    //
    //   * SAME (library, symbol), DIFFERENT signature -> a hard conflict. Two names for one
    //     foreign entry point cannot both be right, and picking either one silently would
    //     miscompile every call site of the other.
    //   * SAME symbol, DIFFERENT libraries -> legal, and the member is flagged
    //     `importRequiresRuntimeBinding`. PE/COFF import libraries key an import by its symbol
    //     NAME, so two libraries claiming one name cannot both be resolved statically -- that
    //     import binds at first call through LoadLibrary/GetProcAddress instead (see
    //     ProjectOopLlvmLowerer::defineRuntimeBoundImports). The static-by-name path is untouched
    //     for every import whose name only one library claims, which is all of them in practice.
    //
    // A `extern C { ... }` declaration with no `from` participates with an EMPTY library: its
    // symbol is resolved by the linker out of an archive, where nothing can be renamed, so a name
    // it claims can never also be claimed dynamically.
    {
        struct ImportIdentity final
        {
            const ClassMemberSymbol* first = nullptr;
            std::string signatureKey;
        };
        std::unordered_map<std::string, ImportIdentity> importsByLibraryAndSymbol;
        std::unordered_map<std::string, std::vector<std::string>> librariesBySymbol;
        const auto signatureOf = [](const ClassMemberSymbol& member)
        {
            std::string key = member.declaredTypeKey + "(";
            for (const MethodParameterSymbol& parameter : member.parameters)
            {
                key += parameter.typeKey;
                key += ',';
            }
            if (member.isVariadic) key += "...";
            key += ')';
            return key;
        };
        for (const ClassMemberSymbol& member : members_)
        {
            if (!member.isImportedC) continue;
            const std::string library = upperAsciiText(member.importDllName);
            const std::string signature = signatureOf(member);
            const auto [iterator, inserted] =
                importsByLibraryAndSymbol.emplace(library + "!" + member.name, ImportIdentity{&member, signature});
            if (!inserted && iterator->second.signatureKey != signature)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidExternCSignature,
                    diagnostics::DiagnosticSeverity::Error,
                    member.range,
                    "conflicting declarations of imported C symbol '" +
                        (member.importDllName.empty() ? member.name
                                                      : member.importDllName + "!" + member.name) +
                        "': an import's identity is (library, symbol, signature)");
                diagnostics.report(
                    diagnostics::DiagnosticId::PreviousDeclaration,
                    diagnostics::DiagnosticSeverity::Note,
                    iterator->second.first->range,
                    "previous declaration of this imported C symbol is here");
                continue;
            }
            if (!inserted) continue;
            std::vector<std::string>& libraries = librariesBySymbol[member.name];
            if (std::find(libraries.begin(), libraries.end(), library) == libraries.end())
            {
                libraries.push_back(library);
            }
        }
        for (ClassMemberSymbol& member : members_)
        {
            if (!member.isImportedC) continue;
            const auto found = librariesBySymbol.find(member.name);
            if (found == librariesBySymbol.end() || found->second.size() < 2) continue;
            if (member.importDllName.empty())
            {
                // The archive side of an ambiguous name: nothing downstream can rename it, so
                // this is the one combination the triple cannot rescue.
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidExternCSignature,
                    diagnostics::DiagnosticSeverity::Error,
                    member.range,
                    "imported C symbol '" + member.name +
                        "' is declared both without a library (resolved from a static archive by "
                        "name) and from a DLL; a statically resolved symbol cannot share its name");
                continue;
            }
            member.importRequiresRuntimeBinding = true;
        }
    }

    for (const ClassMemberSymbol& member : members_)
    {
        if ((member.kind != ClassMemberKind::Method && member.kind != ClassMemberKind::Constructor &&
             member.kind != ClassMemberKind::Destructor) ||
            member.syntax == nullptr || member.syntax->first() == nullptr)
        {
            continue;
        }
        const auto iterator = memberBySyntax_.find(member.syntax);
        if (iterator == memberBySyntax_.end())
        {
            continue;
        }
        scanMethodBody(
            member.syntax->first(),
            member.declaringTypeOrdinal,
            iterator->second,
            diagnostics);
    }

    for (const std::unique_ptr<ProjectSyntaxUnit>& unit : units)
    {
        if (unit != nullptr && unit->syntaxTree() != nullptr)
        {
            scanInvalidThisContexts(unit->syntaxTree(), diagnostics);
        }
    }

    built_ = true;
    return !diagnostics.hasErrors();
}

bool ProjectMemberModel::bindPrecompiledPhysicalNames(
    std::span<const PrecompiledMemberIdentity> identities,
    std::span<const std::pair<std::string, std::string>> typeKeyAliases,
    std::string& error)
{
    error.clear();
    if (!built_ || nameLookup_ == nullptr)
    {
        error = "precompiled physical names require a completed member model";
        return false;
    }

    // Replace the consumer-local generated name of a closed specialization with the producer's
    // published name only while comparing semantic signatures.  The replacement is deliberately
    // longest-first so a nested closed type cannot be partially consumed by an outer alias.
    std::vector<std::pair<std::string, std::string>> aliases(
        typeKeyAliases.begin(), typeKeyAliases.end());
    std::ranges::sort(aliases, [](const auto& left, const auto& right) {
        return left.first.size() > right.first.size();
    });
    const auto canonicalize = [&](std::string value) {
        for (const auto& [localTypeKey, publishedTypeKey] : aliases)
        {
            if (localTypeKey.empty() || localTypeKey == publishedTypeKey) continue;
            std::size_t cursor = 0;
            while ((cursor = value.find(localTypeKey, cursor)) != std::string::npos)
            {
                value.replace(cursor, localTypeKey.size(), publishedTypeKey);
                cursor += publishedTypeKey.size();
            }
        }
        return value;
    };

    for (ClassMemberSymbol& member : members_)
    {
        if (!member.isPrecompiled || member.isImportedC || member.isAbstract ||
            (member.kind != ClassMemberKind::Method && member.kind != ClassMemberKind::Constructor))
        {
            continue;
        }
        if (member.declaringTypeOrdinal >= nameLookup_->namedTypes().size())
        {
            error = "precompiled member '" + member.signatureKey + "' has an invalid declaring type";
            return false;
        }

        const std::string declaringTypeKey = canonicalize(
            "type:" + nameLookup_->namedTypes()[member.declaringTypeOrdinal].identity.key());
        const std::string signatureKey = canonicalize(member.signatureKey);
        const PrecompiledMemberIdentity* match = nullptr;
        for (const PrecompiledMemberIdentity& identity : identities)
        {
            if (identity.mangledName.empty() || identity.kind != member.kind ||
                identity.declaringTypeKey != declaringTypeKey || identity.name != member.name ||
                identity.signatureKey != signatureKey)
            {
                continue;
            }
            if (match != nullptr)
            {
                error = "precompiled callable '" + declaringTypeKey + "." + signatureKey +
                    "' has more than one published physical identity";
                return false;
            }
            match = &identity;
        }
        if (match == nullptr)
        {
            error = "precompiled callable '" + declaringTypeKey + "." + signatureKey +
                "' has no physical identity in NSMETA";
            return false;
        }
        member.precompiledMangledName = match->mangledName;
    }
    return true;
}

std::span<const ClassMemberSymbol> ProjectMemberModel::members() const noexcept
{
    return members_;
}

std::span<const MemberBodyBinding> ProjectMemberModel::bodyBindings() const noexcept
{
    return bodyBindings_;
}

std::span<const std::size_t> ProjectMemberModel::membersOfType(std::size_t typeOrdinal) const noexcept
{
    static const std::vector<std::size_t> empty;
    if (typeOrdinal >= membersByType_.size())
    {
        return empty;
    }
    return membersByType_[typeOrdinal];
}

bool ProjectMemberModel::typeTransitivelyOwnsStorage(std::string_view typeKey) const
{
    if (nameLookup_ == nullptr) return false;

    std::vector<std::optional<bool>> memo(nameLookup_->namedTypes().size());
    std::unordered_set<std::size_t> visiting;
    std::function<bool(std::string_view)> owns = [&](std::string_view candidate) -> bool
    {
        std::string current(candidate);
        while (const std::optional<std::string> element = fixedArrayElementTypeKey(current))
            current = *element;
        const std::optional<std::size_t> ordinal = typeOrdinalForKey(current);
        if (!ordinal.has_value() ||
            nameLookup_->namedTypes()[*ordinal].kind != NamedTypeDeclarationKind::Struct)
            return false;
        if (memo[*ordinal].has_value()) return *memo[*ordinal];
        // Recursive by-value aggregates receive their own incomplete-type diagnostic in layout.
        // Treat the cycle as non-owning here so this safety query does not invent a second cause.
        if (!visiting.insert(*ordinal).second) return false;
        bool result = false;
        for (const std::size_t memberOrdinal : membersByType_[*ordinal])
        {
            if (memberOrdinal >= members_.size()) continue;
            const ClassMemberSymbol& nested = members_[memberOrdinal];
            if (nested.kind != ClassMemberKind::Field || nested.isStatic) continue;
            if (nested.declaredOwnership == sema::OwnershipKind::Owned ||
                owns(nested.declaredTypeKey))
            {
                result = true;
                break;
            }
        }
        visiting.erase(*ordinal);
        memo[*ordinal] = result;
        return result;
    };
    return owns(typeKey);
}

std::span<const std::size_t> ProjectMemberModel::promotedMembersOfType(
    std::size_t typeOrdinal) const noexcept
{
    static const std::vector<std::size_t> empty;
    return typeOrdinal < promotedMembersByType_.size()
        ? std::span<const std::size_t>(promotedMembersByType_[typeOrdinal])
        : std::span<const std::size_t>(empty);
}

std::span<const std::size_t> ProjectMemberModel::findMembers(
    std::size_t typeOrdinal,
    std::string_view name) const noexcept
{
    static const std::vector<std::size_t> empty;
    if (typeOrdinal >= membersByName_.size()) return empty;
    const auto found = membersByName_[typeOrdinal].find(name);
    return found != membersByName_[typeOrdinal].end()
        ? std::span<const std::size_t>(found->second)
        : std::span<const std::size_t>(empty);
}

std::optional<std::size_t> ProjectMemberModel::typeOrdinalForKey(
    std::string_view typeKey) const noexcept
{
    if (!typeKey.starts_with("type:")) return std::nullopt;
    const auto found = typeOrdinalByKey_.find(typeKey.substr(5));
    return found != typeOrdinalByKey_.end()
        ? std::optional<std::size_t>(found->second)
        : std::nullopt;
}

void ProjectMemberModel::buildMemberNameIndex()
{
    membersByName_.clear();
    membersByName_.resize(membersByType_.size());
    for (std::size_t typeOrdinal = 0; typeOrdinal < membersByType_.size(); ++typeOrdinal)
    {
        MemberNameIndex& byName = membersByName_[typeOrdinal];
        for (const std::size_t memberOrdinal : membersByType_[typeOrdinal])
        {
            if (memberOrdinal >= members_.size()) continue;
            const ClassMemberSymbol& member = members_[memberOrdinal];
            if (member.isCompilerGenerated && member.syntax != nullptr &&
                member.syntax->auxiliaryText().find("anonymous-member-carrier") != std::string::npos)
                continue;
            byName[member.name].push_back(memberOrdinal);
        }
        if (typeOrdinal >= promotedMembersByType_.size()) continue;
        for (const std::size_t memberOrdinal : promotedMembersByType_[typeOrdinal])
        {
            if (memberOrdinal < members_.size())
                byName[members_[memberOrdinal].name].push_back(memberOrdinal);
        }
    }
}

bool ProjectMemberModel::isBuilt() const noexcept
{
    return built_;
}

std::string ProjectMemberModel::canonicalTypeKey(
    const ast::TypeSyntax* syntax,
    const ProjectNameLookup& nameLookup) const
{
    if (syntax == nullptr)
    {
        return "<missing-type>";
    }

    std::string result;
    if (lexer::isBuiltinTypeKeyword(syntax->baseTokenKind()))
    {
        result = "builtin:" + syntax->baseName();
    }
    else if (const NamedTypeDeclaration* declaration = nameLookup.resolvedDeclaration(syntax);
             declaration != nullptr)
    {
        result = "type:" + declaration->identity.key();
    }
    else
    {
        result = "unresolved:" + syntax->baseName();
    }

    for (const ast::TypeModifierSyntax& modifier : syntax->modifiers())
    {
        if (modifier.kind == ast::TypeModifierSyntaxKind::Pointer)
        {
            result += "*";
        }
        else if (modifier.kind == ast::TypeModifierSyntaxKind::DynamicArray)
        {
            result += "[]";
        }
        else if (modifier.kind == ast::TypeModifierSyntaxKind::FlexibleArray)
        {
            result += "[*]";
        }
        else
        {
            // C2.1 "fixed arrays": the canonical key carries the COUNT ITSELF -- "builtin:float[4]"
            // -- so every later stage can read the element type and the element count straight off
            // the key. Before this it carried an opaque fingerprint of the count's SYNTAX
            // ("builtin:float[38:4:3:4]"), which nothing downstream could interpret; that is why
            // the type parsed and laid out correctly but indexing such a field never lowered.
            // The fingerprint stays as the fallback for a NON-constant count, which is an error
            // the layout model reports with its own diagnostic -- keeping two declarations with
            // different bad counts distinguishable while that error is being reported.
            if (const std::optional<std::uint64_t> count =
                    evaluateConstantArrayCount(modifier.arrayCount);
                count.has_value())
            {
                result += "[" + std::to_string(*count) + "]";
            }
            else
            {
                result += "[" + syntaxFingerprint(modifier.arrayCount) + "]";
            }
        }
    }
    return result;
}

// C2.1 "fixed arrays". Moved here from ProjectObjectLayoutModel (which now delegates) so the
// compiler has ONE definition of what counts as a constant array count -- the canonical type key
// and the object layout must agree about it, or a field's key and its size would describe
// different arrays.
std::optional<std::uint64_t> ProjectMemberModel::evaluateConstantArrayCount(
    const ast::AstNode* expression) noexcept
{
    if (expression == nullptr) return std::nullopt;
    if (expression->kind() == ast::AstNodeKind::IntegerLiteralExpression ||
        expression->kind() == ast::AstNodeKind::CharacterLiteralExpression)
    {
        return expression->integerValue();
    }
    if (expression->kind() == ast::AstNodeKind::UnaryExpression)
    {
        const std::optional<std::uint64_t> operand = evaluateConstantArrayCount(expression->first());
        if (!operand.has_value()) return std::nullopt;
        switch (expression->tokenKind())
        {
            case lexer::TokenKind::Plus: return operand;
            case lexer::TokenKind::Tilde: return ~*operand;
            case lexer::TokenKind::Minus:
                return *operand == 0 ? std::optional<std::uint64_t>(0) : std::nullopt;
            default: return std::nullopt;
        }
    }
    if (expression->kind() != ast::AstNodeKind::BinaryExpression) return std::nullopt;
    const std::optional<std::uint64_t> left = evaluateConstantArrayCount(expression->first());
    const std::optional<std::uint64_t> right = evaluateConstantArrayCount(expression->second());
    if (!left.has_value() || !right.has_value()) return std::nullopt;
    switch (expression->tokenKind())
    {
        case lexer::TokenKind::Plus:
            return (*left > (std::numeric_limits<std::uint64_t>::max)() - *right)
                ? std::nullopt : std::optional<std::uint64_t>(*left + *right);
        case lexer::TokenKind::Minus:
            return *left >= *right ? std::optional<std::uint64_t>(*left - *right) : std::nullopt;
        case lexer::TokenKind::Star:
            if (*left != 0 && *right > (std::numeric_limits<std::uint64_t>::max)() / *left)
                return std::nullopt;
            return *left * *right;
        case lexer::TokenKind::Slash:
            return *right == 0 ? std::nullopt : std::optional<std::uint64_t>(*left / *right);
        case lexer::TokenKind::Percent:
            return *right == 0 ? std::nullopt : std::optional<std::uint64_t>(*left % *right);
        case lexer::TokenKind::LessLess:
            return *right >= 64 ? std::nullopt : std::optional<std::uint64_t>(*left << *right);
        case lexer::TokenKind::GreaterGreater:
            return *right >= 64 ? std::nullopt : std::optional<std::uint64_t>(*left >> *right);
        case lexer::TokenKind::Amp: return *left & *right;
        case lexer::TokenKind::Pipe: return *left | *right;
        case lexer::TokenKind::Caret: return *left ^ *right;
        default: return std::nullopt;
    }
}

namespace
{
// "builtin:float[4]" -> the position of the '[' that opens the trailing fixed-array suffix, or
// npos when the key does not end in one. A DYNAMIC array key ends in "[]" and is deliberately not
// a fixed array: it is a heap object with a length header, not inline storage.
std::size_t fixedArraySuffixStart(std::string_view typeKey) noexcept
{
    if (typeKey.size() < 3 || typeKey.back() != ']') return std::string_view::npos;
    const std::size_t open = typeKey.rfind('[');
    if (open == std::string_view::npos || open + 1 >= typeKey.size() - 1) return std::string_view::npos;
    for (std::size_t index = open + 1; index + 1 < typeKey.size(); ++index)
    {
        if (std::isdigit(static_cast<unsigned char>(typeKey[index])) == 0)
            return std::string_view::npos;
    }
    return open;
}
}

std::optional<std::string> ProjectMemberModel::fixedArrayElementTypeKey(std::string_view typeKey)
{
    const std::size_t open = fixedArraySuffixStart(typeKey);
    if (open == std::string_view::npos) return std::nullopt;
    return std::string(typeKey.substr(0, open));
}

std::optional<std::uint64_t> ProjectMemberModel::fixedArrayCount(std::string_view typeKey)
{
    const std::size_t open = fixedArraySuffixStart(typeKey);
    if (open == std::string_view::npos) return std::nullopt;
    std::uint64_t value = 0;
    for (std::size_t index = open + 1; index + 1 < typeKey.size(); ++index)
    {
        const std::uint64_t digit = static_cast<std::uint64_t>(typeKey[index] - '0');
        if (value > ((std::numeric_limits<std::uint64_t>::max)() - digit) / 10U) return std::nullopt;
        value = value * 10U + digit;
    }
    return value;
}

std::optional<std::string> ProjectMemberModel::flexibleArrayElementTypeKey(
    std::string_view typeKey)
{
    if (!typeKey.ends_with("[*]") || typeKey.size() <= 3) return std::nullopt;
    return std::string(typeKey.substr(0, typeKey.size() - 3));
}

const char* parameterPassKindName(ParameterPassKind kind) noexcept
{
    switch (kind)
    {
        case ParameterPassKind::Value: return "value";
        case ParameterPassKind::Ref: return "ref";
        case ParameterPassKind::Out: return "out";
        case ParameterPassKind::In: return "in";
    }
    return "value";
}

const char* parameterPassKindKeyword(ParameterPassKind kind) noexcept
{
    switch (kind)
    {
        case ParameterPassKind::Value: return "";
        case ParameterPassKind::Ref: return "ref";
        case ParameterPassKind::Out: return "out";
        case ParameterPassKind::In: return "in";
    }
    return "";
}

std::string ProjectMemberModel::syntaxFingerprint(const ast::AstNode* node) const
{
    if (node == nullptr)
    {
        return "";
    }

    std::ostringstream output;
    output << static_cast<int>(node->kind()) << ':' << node->text() << ':'
           << static_cast<int>(node->tokenKind()) << ':' << node->integerValue();
    if (node->first() != nullptr)
    {
        output << "{f:" << syntaxFingerprint(node->first()) << '}';
    }
    if (node->second() != nullptr)
    {
        output << "{s:" << syntaxFingerprint(node->second()) << '}';
    }
    if (node->third() != nullptr)
    {
        output << "{t:" << syntaxFingerprint(node->third()) << '}';
    }
    for (const ast::AstNode* child : node->children())
    {
        output << "{c:" << syntaxFingerprint(child) << '}';
    }
    return output.str();
}

bool ProjectMemberModel::collectTypeMembers(
    std::size_t typeOrdinal,
    const NamedTypeDeclaration& type,
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::unordered_map<std::string, std::vector<std::size_t>> byName;
    std::unordered_map<std::string, const ast::AstNode*> propertyDeclarations;
    std::unordered_map<std::string, const ast::AstNode*> ordinaryDeclarations;

    for (std::size_t partOrdinal = 0; partOrdinal < type.parts.size(); ++partOrdinal)
    {
        const NamedTypeDeclarationPart& part = type.parts[partOrdinal];
        if (part.syntax == nullptr)
        {
            continue;
        }

        std::size_t declarationOrdinal = 0;
        std::vector<const ast::AstNode*> expandedMembers;
        for (const ast::AstNode* directMember : part.syntax->children())
        {
            const ast::AstNode* declaredMember =
                directMember != nullptr && directMember->kind() == ast::AstNodeKind::StructDeclaration &&
                    directMember->auxiliaryText().starts_with("anonymous-") &&
                    directMember->first() != nullptr
                ? directMember->first()
                : directMember;
            if (directMember != nullptr && directMember->kind() == ast::AstNodeKind::MethodDeclaration &&
                !directMember->genericParameters().empty() &&
                (directMember->hasModifier(lexer::TokenKind::KwExport) ||
                 directMember->hasModifier(lexer::TokenKind::KwExtern)))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    directMember->range(),
                    "Stage 47 export extern C does not expose generic methods or members of generic types");
            }
            if (declaredMember != nullptr && declaredMember->kind() == ast::AstNodeKind::PropertyDeclaration)
            {
                const auto ordinary = ordinaryDeclarations.find(declaredMember->text());
                const auto previousProperty = propertyDeclarations.find(declaredMember->text());
                if (ordinary != ordinaryDeclarations.end() || previousProperty != propertyDeclarations.end())
                {
                    const ast::AstNode* previous = ordinary != ordinaryDeclarations.end()
                        ? ordinary->second : previousProperty->second;
                    diagnostics.report(
                        diagnostics::DiagnosticId::DuplicateSymbol,
                        diagnostics::DiagnosticSeverity::Error,
                        declaredMember->range(),
                        "property name '" + declaredMember->text() + "' conflicts with an existing member in type '" +
                            type.identity.fullyQualifiedName() + "'");
                    if (previous != nullptr)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::PreviousDeclaration,
                            diagnostics::DiagnosticSeverity::Note,
                            previous->range(),
                            "previous declaration is here");
                    }
                }
                else
                {
                    propertyDeclarations.emplace(declaredMember->text(), declaredMember);
                }
            }
            else if (declaredMember != nullptr &&
                     (declaredMember->kind() == ast::AstNodeKind::FieldDeclaration ||
                      declaredMember->kind() == ast::AstNodeKind::EventDeclaration ||
                      declaredMember->kind() == ast::AstNodeKind::MethodDeclaration))
            {
                if (propertyDeclarations.contains(declaredMember->text()))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::DuplicateSymbol,
                        diagnostics::DiagnosticSeverity::Error,
                        declaredMember->range(),
                        "member name '" + declaredMember->text() + "' conflicts with an existing property in type '" +
                            type.identity.fullyQualifiedName() + "'");
                }
                ordinaryDeclarations.emplace(declaredMember->text(), declaredMember);
            }
            appendExpandedMemberNodes(directMember, expandedMembers);
        }
        for (const ast::AstNode* memberNode : expandedMembers)
        {
            if (memberNode == nullptr || !isClassMemberNode(memberNode->kind()))
            {
                continue;
            }

            ClassMemberSymbol symbol;
            if (memberNode->kind() == ast::AstNodeKind::FieldDeclaration)
            {
                symbol.kind = ClassMemberKind::Field;
            }
            else if (memberNode->kind() == ast::AstNodeKind::EventDeclaration)
            {
                symbol.kind = ClassMemberKind::Event;
            }
            else if (memberNode->kind() == ast::AstNodeKind::ConstructorDeclaration)
            {
                symbol.kind = ClassMemberKind::Constructor;
            }
            else if (memberNode->kind() == ast::AstNodeKind::DestructorDeclaration)
            {
                symbol.kind = ClassMemberKind::Destructor;
            }
            else
            {
                symbol.kind = ClassMemberKind::Method;
            }
            symbol.accessibility = memberAccessibility(memberNode);
            symbol.declaringTypeOrdinal = typeOrdinal;
            symbol.partOrdinal = partOrdinal;
            symbol.sourceOrdinal = part.sourceOrdinal;
            symbol.declarationOrdinal = declarationOrdinal++;
            symbol.fileId = part.fileId;
            symbol.range = memberNode->range();
            symbol.syntax = memberNode;
            symbol.sourcePath = part.sourcePath;
            // NSMETA 1.13 may place the producer's real `inline`/`forceinline` body in the
            // synthetic API unit. Those methods are executable input owned by this consumer,
            // unlike the empty declarations beside them. The source modifier is an explicit,
            // format-stable discriminator and also carries the required optimization policy.
            const bool transportedInlineBody =
                part.isPrecompiled && memberNode->kind() == ast::AstNodeKind::MethodDeclaration &&
                (memberNode->hasModifier(lexer::TokenKind::KwInline) ||
                 memberNode->hasModifier(lexer::TokenKind::KwForceInline));
            symbol.isPrecompiled = part.isPrecompiled && !transportedInlineBody;
            symbol.name = memberNode->text();
            symbol.declaredTypeSyntax = memberNode->typeSyntax();
            symbol.declaredTypeKey = symbol.kind == ClassMemberKind::Constructor
                ? "type:" + type.identity.key()
                : symbol.kind == ClassMemberKind::Destructor
                    ? "builtin:void"
                    : canonicalTypeKey(memberNode->typeSyntax(), nameLookup);
            symbol.declaredOwnership =
                (symbol.kind == ClassMemberKind::Constructor || symbol.kind == ClassMemberKind::Destructor)
                ? sema::OwnershipKind::None
                : ownershipKindFromSyntax(memberNode->typeSyntax());
            symbol.isReferenceReturn = symbol.kind == ClassMemberKind::Method && memberNode->isReference();
            if (symbol.isReferenceReturn && symbol.declaredTypeKey == "builtin:void")
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    symbol.range,
                    "ref-returning method '" + symbol.name + "' cannot return void");
            }
            if (symbol.isReferenceReturn && symbol.declaredOwnership != sema::OwnershipKind::None)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    symbol.range,
                    "ref-returning method '" + symbol.name +
                        "' cannot combine ref with owned or borrowed return ownership");
            }
            if (symbol.kind == ClassMemberKind::Field &&
                symbol.declaredOwnership == sema::OwnershipKind::Borrowed)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    symbol.range,
                    "borrowed field '" + symbol.name +
                        "' is rejected in Stage 50 because its lifetime may outlive its source owner");
            }
            // SHR-013: un CAMPO de tipo vista, o de arreglo de vistas, dentro de algo que no es
            // una vista. Es el mismo defecto que la linea de arriba y por eso esta pegado a ella,
            // pero llega por otro camino: alli el prestamo lo dice el calificativo de la
            // declaracion, aqui lo dice el TIPO, y el calificativo no aparece por ningun lado.
            //
            // La razon por la que no hay grado intermedio: un campo vive tanto como su objeto, y
            // ese objeto no tiene relacion alguna con el duenno de lo prestado. No hay
            // comprobacion que hacer -- no hay dos tiempos de vida que comparar, solo uno
            // conocido y otro cualquiera -- asi que la respuesta honesta es no dejarlo declarar.
            //
            // La EXCEPCION es una vista dentro de otra vista, y no es una concesion: componer
            // dos vistas da una vista, y el valor compuesto sigue muriendo donde muera el que lo
            // contiene, que es un local o un parametro y esta comprobado como tal.
            if (symbol.kind == ClassMemberKind::Field &&
                typeKeyIsViewLike(symbol.declaredTypeKey, nameLookup) &&
                !namedTypeIsBorrowedView(&type))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    symbol.range,
                    "field '" + symbol.name + "' cannot have view type '" + symbol.declaredTypeKey +
                        "': a field outlives every call, and a view borrows from something that "
                        "does not. Store the source and build the view where it is used");
            }
            symbol.isConst = symbol.kind == ClassMemberKind::Field &&
                memberNode->hasModifier(lexer::TokenKind::KwConst);
            symbol.inliningPolicy = symbol.kind == ClassMemberKind::Method &&
                    memberNode->hasModifier(lexer::TokenKind::KwNoInline)
                ? InliningPolicy::Forbidden
                : symbol.kind == ClassMemberKind::Method &&
                        memberNode->hasModifier(lexer::TokenKind::KwForceInline)
                ? InliningPolicy::Required
                : symbol.kind == ClassMemberKind::Method &&
                        memberNode->hasModifier(lexer::TokenKind::KwInline)
                    ? InliningPolicy::Preferred
                    : InliningPolicy::Automatic;
            symbol.isNoHeap = (symbol.kind == ClassMemberKind::Method ||
                               symbol.kind == ClassMemberKind::Constructor) &&
                memberNode->hasModifier(lexer::TokenKind::KwNoHeap);
            symbol.isStatic = (symbol.kind == ClassMemberKind::Field || symbol.kind == ClassMemberKind::Event ||
                symbol.kind == ClassMemberKind::Method) &&
                (memberNode->hasModifier(lexer::TokenKind::KwStatic) || symbol.isConst);
            symbol.isNewSlot = memberNode->hasModifier(lexer::TokenKind::KwNew);
            const bool readonlyDeclaringStruct = type.kind == NamedTypeDeclarationKind::Struct &&
                type.syntax != nullptr &&
                type.syntax->hasModifier(lexer::TokenKind::KwReadonly);
            const bool readonlyPropertyBacking = readonlyDeclaringStruct &&
                memberNode->auxiliaryText().starts_with("property-backing:");
            symbol.isReadonly =
                (symbol.kind == ClassMemberKind::Field &&
                 (memberNode->hasModifier(lexer::TokenKind::KwReadonly) || readonlyPropertyBacking)) ||
                (symbol.kind == ClassMemberKind::Method && !symbol.isStatic &&
                 (memberNode->hasModifier(lexer::TokenKind::KwReadonly) || readonlyDeclaringStruct));
            symbol.isVolatile = symbol.kind == ClassMemberKind::Field &&
                memberNode->hasModifier(lexer::TokenKind::KwVolatile);
            if (symbol.isVolatile &&
                (!isVolatileFieldType(symbol.declaredTypeKey, nameLookup) ||
                 symbol.declaredOwnership != sema::OwnershipKind::None))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    symbol.range,
                    "volatile field '" + symbol.name +
                        "' must be a non-owning native scalar, enum, or raw pointer; volatile "
                        "does not define aggregate-copy or managed-reference semantics");
            }
            if (symbol.isVolatile && (symbol.isReadonly || symbol.isConst))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    symbol.range,
                    "volatile field '" + symbol.name + "' cannot also be readonly or const");
            }
            if (type.syntax != nullptr &&
                type.syntax->hasModifier(lexer::TokenKind::KwReadonly) &&
                symbol.kind == ClassMemberKind::Field && !symbol.isStatic && !symbol.isReadonly)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    symbol.range,
                    "every instance field of readonly struct '" + type.identity.name() +
                        "' must be declared readonly");
            }
            // Bloque 1: a STATIC field cannot be `owned`, and this has to be said here rather than
            // left to fail downstream.
            //
            // Ownership in this language is discharged by a destruction entry: an instance field
            // declared `owned` is released when its containing object dies (see
            // ProjectOopMirDispatchTable::ownedFields). A static field has no containing object and
            // no end of life -- nothing runs at process exit -- so an `owned` static could only
            // ever be a leak with an ownership annotation on it.
            //
            // Without this the declaration was accepted and the failure surfaced at the first
            // `Slot = move(x)` as "move(...) is only valid for a direct RAII local or owned
            // parameter...", which points at the transfer and says nothing about the `static` that
            // actually caused it -- a diagnostic that sends the reader to fix the wrong line. The
            // check also covers the compiler-generated backing field of a `static owned` automatic
            // property, whose range is the property's own name.
            if (symbol.kind == ClassMemberKind::Field && symbol.isStatic &&
                symbol.declaredOwnership == sema::OwnershipKind::Owned)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    symbol.range,
                    "static field '" + symbol.name +
                        "' cannot be owned because no destruction ever releases it; ownership is "
                        "discharged by the containing object's destruction");
            }
            symbol.declaringTypeIsStruct = type.kind == NamedTypeDeclarationKind::Struct;
            symbol.hasImplicitThis = symbol.kind == ClassMemberKind::Constructor ||
                symbol.kind == ClassMemberKind::Destructor ||
                (symbol.kind == ClassMemberKind::Method && !symbol.isStatic);
            symbol.isAbstract = symbol.kind == ClassMemberKind::Method &&
                memberNode->hasModifier(lexer::TokenKind::KwAbstract);
            symbol.isSealed = symbol.kind == ClassMemberKind::Method &&
                memberNode->hasModifier(lexer::TokenKind::KwSealed);
            symbol.isVirtual = symbol.kind == ClassMemberKind::Method &&
                (memberNode->hasModifier(lexer::TokenKind::KwVirtual) || symbol.isAbstract);
            symbol.isOverride = symbol.kind == ClassMemberKind::Method &&
                memberNode->hasModifier(lexer::TokenKind::KwOverride);
            symbol.isExported = symbol.kind == ClassMemberKind::Method &&
                memberNode->hasModifier(lexer::TokenKind::KwExport);
            symbol.isExternC = symbol.kind == ClassMemberKind::Method &&
                memberNode->hasModifier(lexer::TokenKind::KwExtern);
            symbol.isVariadic = symbol.kind == ClassMemberKind::Method && memberNode->isVariadic();
            // Interop Foundation: a member born inside `extern C from "<dll>" { ... }` is an
            // IMPORTED C function -- the class-side face of a symbol that lives in an external
            // DLL. Its C symbol is the member name verbatim; the (dll, symbol) pair flows to the
            // linker; and it deliberately has NO body anywhere in the project.
            if (symbol.kind == ClassMemberKind::Method &&
                memberNode->auxiliaryText().starts_with("cimport:"))
            {
                symbol.isImportedC = true;
                symbol.importDllName = memberNode->auxiliaryText().substr(std::string("cimport:").size());
                symbol.cAbiName = symbol.name;
            }
            symbol.isExportedC = symbol.isExported && symbol.isExternC;
            if (symbol.isVariadic && !symbol.isImportedC)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidExternCSignature,
                    diagnostics::DiagnosticSeverity::Error,
                    symbol.range,
                    "a variadic callable must be a body-less extern C import declaration");
            }
            if (symbol.isReferenceReturn && (symbol.isExportedC || symbol.isImportedC))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    symbol.range,
                    "ref returns are a NativeSharp ABI contract and cannot cross an extern C boundary");
            }
            if (symbol.isExportedC) symbol.cAbiName = symbol.name;

            // C2.2 "union and explicit layout": `at(N)` on a field. Only an INSTANCE field of a
            // struct or union has a byte offset to name -- a static field is a global, and a
            // class field sits after a header this compiler owns -- so anything else is a
            // diagnostic rather than a silently ignored modifier.
            if (memberNode->auxiliaryText().starts_with("at:"))
            {
                const std::string offsetText =
                    memberNode->auxiliaryText().substr(std::string("at:").size());
                symbol.explicitFieldOffset = std::stoull(offsetText);
                if (symbol.kind != ClassMemberKind::Field || symbol.isStatic ||
                    type.kind != NamedTypeDeclarationKind::Struct)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        symbol.range,
                        "an explicit field offset applies only to an instance field of a struct or union");
                    symbol.explicitFieldOffset.reset();
                }
            }
            if (const std::size_t marker = memberNode->auxiliaryText().find("field-align:");
                marker != std::string::npos)
            {
                const std::size_t valueBegin = marker + std::string("field-align:").size();
                symbol.explicitFieldAlignment =
                    std::stoull(memberNode->auxiliaryText().substr(valueBegin));
                if (symbol.kind != ClassMemberKind::Field || symbol.isStatic)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        symbol.range,
                        "explicit field alignment applies only to an instance field");
                    symbol.explicitFieldAlignment.reset();
                }
            }

            const std::string& metadata = memberNode->auxiliaryText();
            symbol.isCompilerGenerated = metadata.starts_with("property-backing:") ||
                metadata.starts_with("event-backing:") ||
                metadata.find("anonymous-member-carrier") != std::string::npos ||
                metadata.find("bitfield-unnamed") != std::string::npos;
            symbol.isBitField = metadata.find("bitfield") != std::string::npos;
            symbol.isUnnamedBitField = metadata.find("bitfield-unnamed") != std::string::npos;
            symbol.bitWidthSyntax = symbol.isBitField ? memberNode->second() : nullptr;
            symbol.isEvent = symbol.kind == ClassMemberKind::Event;
            symbol.isEventBacking = metadata.starts_with("event-backing:");
            symbol.isPropertyAccessor = metadata.starts_with("property:get:") ||
                metadata.starts_with("property:set:");
            symbol.isIndexerAccessor = metadata.starts_with("indexer:get:") ||
                metadata.starts_with("indexer:set:");
            symbol.isGetter = metadata.starts_with("property:get:") ||
                metadata.starts_with("indexer:get:");
            symbol.isSetter = metadata.starts_with("property:set:") ||
                metadata.starts_with("indexer:set:");
            symbol.isUserOperator = metadata.starts_with("operator:");
            symbol.isUserConversion = metadata.starts_with("conversion:");
            symbol.isImplicitUserConversion = metadata == "conversion:implicit";

            // Bloque 1 item 9: a GETTER of an `owned`-declared property/indexer returns a
            // borrow, not ownership.
            //
            // The parser gives the getter accessor the property's own TypeSyntax as its return
            // type (Parser::parsePropertyTail / parseIndexerTail), so a declaration written
            // `public owned Res Slot { get { return backing; } set { backing = move(value); } }`
            // arrives here as a method whose declared return ownership is Owned. Left alone,
            // ProjectMethodBodyModel::analyzeReturn then demands an ownership-transfer
            // expression from the getter -- "owned return requires an ownership-transfer
            // expression such as move(owner), new T(...), or an owned-returning call" -- which
            // no correct getter can ever supply: the only thing it could move out is the field
            // the object still owns and still destroys.
            //
            // That diagnostic firing on every well-formed getter was the reason `owned` was
            // undeclarable on a property at all. Rather than special-case the check downstream,
            // the ownership is resolved once, here, at the point where the property's shape is
            // still visible: the getter's return is an ordinary (unqualified) reference, and the
            // property's ownership is remembered on `ownsAccessorStorage` so the assignment side
            // can still recognize the setter as a legitimate move(...) destination.
            //
            // Deliberately NOT `Borrowed`: a borrowed return in this language carries a proven
            // lifetime binding back to a live owner local or borrowed parameter, and a field read
            // has no such provenance to offer (measured: `borrowed Res Peek() { return backing; }`
            // is rejected with "borrowed return requires a borrowed value whose lifetime
            // outlives the call"). An unqualified return is what every read-only container
            // accessor in the library already returns today.
            if ((symbol.isPropertyAccessor || symbol.isIndexerAccessor) && symbol.isGetter &&
                symbol.declaredOwnership == sema::OwnershipKind::Owned)
            {
                symbol.ownsAccessorStorage = true;
                symbol.declaredOwnership = sema::OwnershipKind::None;
            }
            if (symbol.isPropertyAccessor)
            {
                const std::size_t prefix = symbol.isGetter
                    ? std::string("property:get:").size()
                    : std::string("property:set:").size();
                symbol.associatedName = metadata.substr(prefix);
            }
            else if (symbol.isIndexerAccessor)
            {
                symbol.associatedName = "Item";
            }
            else if (symbol.isEventBacking)
            {
                symbol.associatedName = metadata.substr(std::string("event-backing:").size());
            }
            else if (symbol.isCompilerGenerated)
            {
                symbol.associatedName = metadata.substr(std::string("property-backing:").size());
            }

            if (symbol.isEvent)
            {
                const NamedTypeDeclaration* eventType = nameLookup.resolvedDeclaration(symbol.declaredTypeSyntax);
                if (eventType == nullptr || eventType->kind != NamedTypeDeclarationKind::Delegate)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        symbol.range,
                        "Stage 42 event '" + symbol.name + "' must have a delegate type");
                }
                else if (eventType->syntax != nullptr &&
                         canonicalTypeKey(eventType->syntax->typeSyntax(), nameLookup) != "builtin:void")
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        symbol.range,
                        "Stage 42 events require a void-returning delegate type");
                }
            }

            if (symbol.kind == ClassMemberKind::Method || symbol.kind == ClassMemberKind::Constructor)
            {
                std::unordered_map<std::string, const ast::AstNode*> parameterNames;
                bool sawOptionalParameter = false;
                bool sawParameterPack = false;
                for (const ast::AstNode* parameterNode : memberNode->children())
                {
                    if (parameterNode == nullptr ||
                        parameterNode->kind() != ast::AstNodeKind::ParameterDeclaration)
                    {
                        continue;
                    }

                    MethodParameterSymbol parameter;
                    parameter.name = parameterNode->text();
                    parameter.typeSyntax = parameterNode->typeSyntax();
                    parameter.typeKey = canonicalTypeKey(parameterNode->typeSyntax(), nameLookup);
                    parameter.ownership = ownershipKindFromSyntax(parameterNode->typeSyntax());
                    parameter.fileId = part.fileId;
                    parameter.range = parameterNode->range();
                    parameter.syntax = parameterNode;
                    parameter.defaultValueSyntax = parameterNode->first();
                    parameter.isParameterPack = parameterNode->auxiliaryText() == "params";
                    if (sawParameterPack)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidType,
                            diagnostics::DiagnosticSeverity::Error,
                            parameter.range,
                            "a typed parameter pack must be the final parameter");
                    }
                    if (parameter.isParameterPack)
                    {
                        sawParameterPack = true;
                        if (!language::isTypedParameterPackType(parameter.typeKey))
                        {
                            diagnostics.report(
                                diagnostics::DiagnosticId::InvalidType,
                                diagnostics::DiagnosticSeverity::Error,
                                parameter.range,
                                "params requires ReadOnlySpan<T>; implicit heap arrays are forbidden");
                        }
                        if (parameter.defaultValueSyntax != nullptr)
                        {
                            diagnostics.report(
                                diagnostics::DiagnosticId::InvalidOperand,
                                diagnostics::DiagnosticSeverity::Error,
                                parameter.range,
                                "a typed parameter pack cannot declare a default value");
                        }
                    }
                    // C4 "language references": the parser already turned the written type into a
                    // pointer, so `typeKey` is `T*`. `pointeeTypeKey` is what the programmer
                    // wrote, and it is what a USE of the parameter inside the body evaluates to --
                    // that implicit dereference is the whole ergonomic difference between `ref int
                    // x` and `int* x`.
                    if (parameterNode->auxiliaryText().starts_with("pass:"))
                    {
                        const std::string spelling =
                            parameterNode->auxiliaryText().substr(std::string("pass:").size());
                        parameter.passKind = spelling == "ref"
                            ? ParameterPassKind::Ref
                            : (spelling == "out" ? ParameterPassKind::Out : ParameterPassKind::In);
                        parameter.pointeeTypeKey = parameter.typeKey.empty() || parameter.typeKey.back() != '*'
                            ? parameter.typeKey
                            : parameter.typeKey.substr(0, parameter.typeKey.size() - 1);
                    }

                    if (parameter.defaultValueSyntax != nullptr)
                    {
                        sawOptionalParameter = true;
                        const ast::AstNodeKind defaultKind = parameter.defaultValueSyntax->kind();
                        const bool literal = defaultKind == ast::AstNodeKind::IntegerLiteralExpression ||
                            defaultKind == ast::AstNodeKind::FloatingLiteralExpression ||
                            defaultKind == ast::AstNodeKind::CharacterLiteralExpression ||
                            defaultKind == ast::AstNodeKind::StringLiteralExpression ||
                            defaultKind == ast::AstNodeKind::BooleanLiteralExpression ||
                            defaultKind == ast::AstNodeKind::NullLiteralExpression;
                        const bool signedLiteral = defaultKind == ast::AstNodeKind::UnaryExpression &&
                            (parameter.defaultValueSyntax->tokenKind() == lexer::TokenKind::Plus ||
                             parameter.defaultValueSyntax->tokenKind() == lexer::TokenKind::Minus) &&
                            parameter.defaultValueSyntax->first() != nullptr &&
                            (parameter.defaultValueSyntax->first()->kind() ==
                                 ast::AstNodeKind::IntegerLiteralExpression ||
                             parameter.defaultValueSyntax->first()->kind() ==
                                 ast::AstNodeKind::FloatingLiteralExpression);
                        if (!literal && !signedLiteral)
                        {
                            diagnostics.report(
                                diagnostics::DiagnosticId::InvalidOperand,
                                diagnostics::DiagnosticSeverity::Error,
                                parameter.defaultValueSyntax->range(),
                                "default parameter value must be a literal constant");
                        }
                        else
                        {
                            const ast::AstNode* valueSyntax = signedLiteral
                                ? parameter.defaultValueSyntax->first()
                                : parameter.defaultValueSyntax;
                            const ast::AstNodeKind valueKind = valueSyntax->kind();
                            bool compatible = false;
                            if (valueKind == ast::AstNodeKind::IntegerLiteralExpression ||
                                valueKind == ast::AstNodeKind::CharacterLiteralExpression)
                            {
                                compatible = ProjectTypeConversion::isNumeric(parameter.typeKey) &&
                                    parameter.typeKey != "builtin:bool";
                            }
                            else if (valueKind == ast::AstNodeKind::FloatingLiteralExpression)
                            {
                                compatible = parameter.typeKey == "builtin:float" ||
                                    parameter.typeKey == "builtin:double";
                            }
                            else if (valueKind == ast::AstNodeKind::BooleanLiteralExpression)
                            {
                                compatible = parameter.typeKey == "builtin:bool";
                            }
                            else if (valueKind == ast::AstNodeKind::StringLiteralExpression)
                            {
                                compatible = parameter.typeKey == "builtin:string" ||
                                    parameter.typeKey == "builtin:char*";
                            }
                            else if (valueKind == ast::AstNodeKind::NullLiteralExpression)
                            {
                                const NamedTypeDeclaration* named =
                                    namedTypeForKey(parameter.typeKey, nameLookup);
                                compatible = parameter.typeKey == "builtin:string" ||
                                    ProjectTypeConversion::isPointer(parameter.typeKey) ||
                                    ProjectTypeConversion::isNativeArray(parameter.typeKey) ||
                                    (named != nullptr &&
                                     (named->kind == NamedTypeDeclarationKind::Class ||
                                      named->kind == NamedTypeDeclarationKind::Interface ||
                                      named->kind == NamedTypeDeclarationKind::Delegate));
                            }
                            if (!compatible)
                            {
                                diagnostics.report(
                                    diagnostics::DiagnosticId::InvalidConversion,
                                    diagnostics::DiagnosticSeverity::Error,
                                    parameter.defaultValueSyntax->range(),
                                    "default value cannot convert to parameter type '" +
                                        parameter.typeKey + "'");
                            }
                        }
                        if (parameter.passKind != ParameterPassKind::Value ||
                            parameter.ownership != sema::OwnershipKind::None)
                        {
                            diagnostics.report(
                                diagnostics::DiagnosticId::InvalidOperand,
                                diagnostics::DiagnosticSeverity::Error,
                                parameter.range,
                                "default values are allowed only on unqualified value parameters");
                        }
                    }
                    else if (sawOptionalParameter)
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidOperand,
                            diagnostics::DiagnosticSeverity::Error,
                            parameter.range,
                            "required parameter '" + parameter.name +
                                "' cannot follow a parameter with a default value");
                    }

                    const std::string callableKind = symbol.kind == ClassMemberKind::Constructor
                        ? "constructor"
                        : "method";
                    if (isInvalidVoidObjectType(parameter.typeSyntax))
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidType,
                            diagnostics::DiagnosticSeverity::Error,
                            parameter.range,
                            callableKind + " parameter '" + parameter.name +
                                "' cannot have object type 'void'; use a pointer type such as void* when appropriate");
                    }

                    const auto previousParameter = parameterNames.find(parameter.name);
                    if (previousParameter != parameterNames.end())
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::DuplicateSymbol,
                            diagnostics::DiagnosticSeverity::Error,
                            parameter.range,
                            "duplicate " + callableKind + " parameter '" + parameter.name + "' in member '" +
                                symbol.name + "'");
                        diagnostics.report(
                            diagnostics::DiagnosticId::PreviousDeclaration,
                            diagnostics::DiagnosticSeverity::Note,
                            previousParameter->second->range(),
                            "previous parameter declaration is here");
                    }
                    else
                    {
                        parameterNames.emplace(parameter.name, parameterNode);
                    }
                    symbol.parameters.push_back(std::move(parameter));
                }

                // C4: a by-reference parameter is part of the signature. `F(int)` and
                // `F(ref int)` are different members, and the signature key has to say so or the
                // two would collide as duplicates.
                symbol.signatureKey = symbol.name + '(';
                for (std::size_t parameterIndex = 0;
                     parameterIndex < symbol.parameters.size();
                     ++parameterIndex)
                {
                    if (parameterIndex != 0)
                    {
                        symbol.signatureKey.push_back(',');
                    }
                    symbol.signatureKey += ownershipSignaturePrefix(
                        symbol.parameters[parameterIndex].ownership);
                    // C4: `F(int* p)` and `F(ref int p)` lower to the same machine signature but
                    // are different members of the language -- the second one demands the keyword
                    // at every call site. Without this prefix the two would collide as duplicates.
                    if (symbol.parameters[parameterIndex].passKind != ParameterPassKind::Value)
                    {
                        symbol.signatureKey +=
                            parameterPassKindKeyword(symbol.parameters[parameterIndex].passKind);
                        symbol.signatureKey.push_back(' ');
                    }
                    symbol.signatureKey += symbol.parameters[parameterIndex].typeKey;
                }
                symbol.signatureKey.push_back(')');
                if (symbol.isVariadic)
                {
                    symbol.signatureKey.insert(symbol.signatureKey.size() - 1, ",...");
                }
                // Unlike an ordinary method, a conversion operator is identified by its
                // source AND target types. Multiple conversions may therefore share
                // `op_Implicit(Source)` while producing distinct target types.
                if (symbol.isUserConversion)
                {
                    symbol.signatureKey += "->" + symbol.declaredTypeKey;
                }

                // Bloque 1 item 9, setter half. The setter's implicit `value` parameter is
                // built from the property's own TypeSyntax, so it already carries the `owned`
                // qualifier and already behaves as an owned parameter inside the accessor body
                // -- `backing = move(value)` compiles unchanged. What was missing is the
                // CALL SITE: `h.Slot = move(a)` was rejected because the assignment target was
                // neither a RAII local nor an array element nor an owned field.
                //
                // Marking the setter lets ProjectMethodBodyModel::analyzeAssignment recognize it
                // as a fourth legitimate move(...) destination. The `value` parameter is always
                // appended last (after any index parameters, for an indexer), which is what makes
                // `parameters.back()` the right one to read.
                if ((symbol.isPropertyAccessor || symbol.isIndexerAccessor) && symbol.isSetter &&
                    !symbol.parameters.empty() &&
                    symbol.parameters.back().ownership == sema::OwnershipKind::Owned)
                {
                    symbol.ownsAccessorStorage = true;
                }
            }
            else if (symbol.kind == ClassMemberKind::Destructor)
            {
                symbol.signatureKey = symbol.name + "()";
            }
            else
            {
                symbol.signatureKey = symbol.name;
            }

            // C2.1 "fixed arrays": a fixed array is INLINE STORAGE, not a value that can be
            // copied across a call. C says the same thing by decaying an array parameter to a
            // pointer and by refusing to return one at all, and NativeSharp says it out loud
            // instead of silently passing 16 bytes by value: the honest spelling for both is a
            // raw pointer to the element type. A FIELD, a LOCAL and an element read/write are
            // exactly where a fixed array does belong, and all three work.
            if (symbol.kind == ClassMemberKind::Method)
            {
                if (fixedArrayElementTypeKey(symbol.declaredTypeKey).has_value() ||
                    flexibleArrayElementTypeKey(symbol.declaredTypeKey).has_value())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        symbol.range,
                        "an inline array cannot be returned by value; return a pointer to its element type");
                }
                for (const MethodParameterSymbol& parameter : symbol.parameters)
                {
                    if (!fixedArrayElementTypeKey(parameter.typeKey).has_value() &&
                        !flexibleArrayElementTypeKey(parameter.typeKey).has_value()) continue;
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        parameter.range,
                        "an inline array cannot be passed by value; declare the parameter as a pointer to its element type");
                }
            }

            if (symbol.isExported || symbol.isExternC)
            {
                // Interop Foundation: an IMPORTED C function (`extern C from "<dll>"`) shares the
                // C-ABI surface rules with exports -- same type validation below, same reserved-
                // name protection -- but is deliberately NOT an export: it has no body here, its
                // definition lives in the named DLL, and it must not demand the `export` half of
                // the Stage 47 pairing.
                if (!symbol.isExportedC && !symbol.isImportedC)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        symbol.range,
                        "Stage 47 exported host methods require the exact paired modifiers 'export extern C'");
                }
                if (symbol.kind != ClassMemberKind::Method || symbol.accessibility != ClassMemberAccessibility::Public ||
                    !symbol.isStatic || symbol.isAbstract || symbol.isCompilerGenerated || symbol.isPropertyAccessor ||
                    symbol.isIndexerAccessor || symbol.isUserOperator || symbol.isUserConversion)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        symbol.range,
                        "Stage 47 export extern C requires an ordinary public static concrete method");
                }
                if (!memberNode->genericParameters().empty() ||
                    (type.syntax != nullptr && !type.syntax->genericParameters().empty()))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        symbol.range,
                        "Stage 47 export extern C does not expose generic methods or members of generic types");
                }
                if (isReservedStage47CAbiExportName(symbol.cAbiName))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        symbol.range,
                        "Stage 47 C ABI export name '" + symbol.cAbiName +
                            "' collides with a reserved NativeSharp/runtime/toolchain symbol");
                }
                // C1.1 "function-pointer round trip": an ORDINARY delegate is a 16-byte heap cell
                // (code + receiver) and has no C shape, so returning one across the boundary stays
                // rejected. An EXTERN C delegate is a raw C function pointer end to end (see
                // lowerDelegateCreate: binding a static method to one yields the function's own
                // address), so it is exactly as returnable as `void*` -- which is what makes
                // `wglGetProcAddress`/`SDL_GL_GetProcAddress`-shaped declarations expressible with
                // their real signature instead of an untyped `nint` the caller must blind-cast.
                const NamedTypeDeclaration* exportedReturnType = namedTypeForKey(symbol.declaredTypeKey, nameLookup);
                if (exportedReturnType != nullptr &&
                    exportedReturnType->kind == NamedTypeDeclarationKind::Delegate &&
                    !(exportedReturnType->syntax != nullptr &&
                      exportedReturnType->syntax->hasModifier(lexer::TokenKind::KwExtern)))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidExternCSignature,
                        diagnostics::DiagnosticSeverity::Error,
                        symbol.range,
                        "Stage 47 C ABI does not return delegate values; return an extern C delegate (a raw function pointer) instead");
                }
                if (!isStage47CAbiType(symbol.declaredTypeKey, nameLookup, true))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidExternCSignature,
                        diagnostics::DiagnosticSeverity::Error,
                        symbol.range,
                        "Stage 47 C ABI cannot export return type '" + symbol.declaredTypeKey +
                            "'; use scalars, enums, small plain structs, raw pointers or extern C delegates");
                }
                for (const MethodParameterSymbol& parameter : symbol.parameters)
                {
                    if (!isStage47CAbiType(parameter.typeKey, nameLookup, false))
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidExternCSignature,
                            diagnostics::DiagnosticSeverity::Error,
                            parameter.range,
                            "Stage 47 C ABI cannot export parameter type '" + parameter.typeKey +
                                "'; pass strings/arrays through explicit pointer+length boundaries; class/interface references are opaque handles");
                    }
                }
            }

            if (symbol.isUserOperator)
            {
                const std::string containingTypeKey = "type:" + type.identity.key();
                bool containsContainingType = false;
                for (const MethodParameterSymbol& parameter : symbol.parameters)
                {
                    containsContainingType = containsContainingType || parameter.typeKey == containingTypeKey;
                }
                if (!containsContainingType)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        symbol.range,
                        "user-defined operator '" + symbol.name +
                            "' must declare at least one operand of the containing type");
                }
                const bool comparison = metadata == "operator:EqualEqual" ||
                    metadata == "operator:BangEqual" || metadata == "operator:Less" ||
                    metadata == "operator:LessEqual" || metadata == "operator:Greater" ||
                    metadata == "operator:GreaterEqual";
                if (comparison && symbol.declaredTypeKey != "builtin:bool")
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        symbol.range,
                        "comparison/equality user-defined operators must return bool in Stage 38");
                }
            }

            if (symbol.isUserConversion && symbol.parameters.size() == 1)
            {
                const std::string containingTypeKey = "type:" + type.identity.key();
                const std::string& sourceTypeKey = symbol.parameters.front().typeKey;
                const std::string& targetTypeKey = symbol.declaredTypeKey;
                if (sourceTypeKey == targetTypeKey)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        symbol.range,
                        "user-defined conversion source and target types must differ");
                }
                if (sourceTypeKey != containingTypeKey && targetTypeKey != containingTypeKey)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        symbol.range,
                        "user-defined conversion must convert to or from the containing type");
                }
            }

            bool duplicate = false;
            const auto sameName = byName.find(symbol.name);
            if (sameName != byName.end())
            {
                for (const std::size_t previousOrdinal : sameName->second)
                {
                    if (previousOrdinal >= members_.size())
                    {
                        continue;
                    }
                    const ClassMemberSymbol& previous = members_[previousOrdinal];
                    if (symbol.kind == ClassMemberKind::Field || symbol.kind == ClassMemberKind::Event ||
                        previous.kind == ClassMemberKind::Field || previous.kind == ClassMemberKind::Event)
                    {
                        reportDuplicate(
                            symbol,
                            previous,
                            diagnostics,
                            "member name '" + symbol.name +
                                "' conflicts with an existing member in class '" +
                                type.identity.fullyQualifiedName() + "'");
                        duplicate = true;
                        break;
                    }
                    if (symbol.kind == previous.kind && symbol.signatureKey == previous.signatureKey)
                    {
                        const std::string category = symbol.kind == ClassMemberKind::Constructor
                            ? "constructor"
                            : symbol.kind == ClassMemberKind::Destructor
                                ? "destructor"
                                : "method";
                        reportDuplicate(
                            symbol,
                            previous,
                            diagnostics,
                            "duplicate " + category + " signature '" + symbol.signatureKey +
                                "' in type '" + type.identity.fullyQualifiedName() +
                                (symbol.kind == ClassMemberKind::Method && !symbol.isUserConversion
                                    ? "'; return type is not part of overload identity"
                                    : "'"));
                        duplicate = true;
                        break;
                    }
                }
            }

            if (duplicate)
            {
                continue;
            }

            const std::size_t memberOrdinal = members_.size();
            members_.push_back(std::move(symbol));
            membersByType_[typeOrdinal].push_back(memberOrdinal);
            membersByName_[typeOrdinal][members_[memberOrdinal].name].push_back(memberOrdinal);
            memberBySyntax_.emplace(memberNode, memberOrdinal);
            byName[members_[memberOrdinal].name].push_back(memberOrdinal);
        }
    }

    return !diagnostics.hasErrors();
}

bool ProjectMemberModel::collectAnonymousMemberPromotions(
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // Item 326. Promotion is a semantic alias, not physical flattening. Every unnamed aggregate
    // still owns one compiler-generated carrier field so its pack/align/size, union overlap and
    // tail padding remain authoritative. The aliases live outside membersByType_: ordinary model
    // consumers therefore continue to see exactly the physical fields, while findMembers() adds
    // the promoted surface for source resolution.
    const auto intersectAccessibility = [](
        ClassMemberAccessibility carrier,
        ClassMemberAccessibility member) noexcept
    {
        if (carrier == member) return carrier;
        if (carrier == ClassMemberAccessibility::Private ||
            member == ClassMemberAccessibility::Private)
            return ClassMemberAccessibility::Private;
        if (carrier == ClassMemberAccessibility::Public) return member;
        if (member == ClassMemberAccessibility::Public) return carrier;
        // `internal` intersected with `protected` has no single NativeSharp spelling. Private is
        // the conservative result: promotion must never make a member more visible than either
        // edge of its path.
        return ClassMemberAccessibility::Private;
    };

    enum class VisitState { Unvisited, Visiting, Complete };
    std::vector<VisitState> states(nameLookup.namedTypes().size(), VisitState::Unvisited);
    std::function<void(std::size_t)> visit = [&](std::size_t typeOrdinal)
    {
        if (typeOrdinal >= states.size() || states[typeOrdinal] == VisitState::Complete) return;
        if (states[typeOrdinal] == VisitState::Visiting) return;
        states[typeOrdinal] = VisitState::Visiting;

        std::unordered_map<std::string, std::size_t> directNames;
        for (const std::size_t ordinal : membersByType_[typeOrdinal])
        {
            if (ordinal >= members_.size()) continue;
            const ClassMemberSymbol& member = members_[ordinal];
            if (!member.isCompilerGenerated) directNames.emplace(member.name, ordinal);
        }
        std::unordered_map<std::string, std::size_t> promotedNames;
        const std::vector<std::size_t> physicalMembers = membersByType_[typeOrdinal];
        for (const std::size_t carrierOrdinal : physicalMembers)
        {
            if (carrierOrdinal >= members_.size()) continue;
            const ClassMemberSymbol& carrierBeforeVisit = members_[carrierOrdinal];
            if (carrierBeforeVisit.kind != ClassMemberKind::Field || carrierBeforeVisit.isStatic ||
                carrierBeforeVisit.syntax == nullptr ||
                carrierBeforeVisit.syntax->auxiliaryText().find("anonymous-member-carrier") ==
                    std::string::npos)
                continue;

            const std::optional<std::size_t> nestedTypeOrdinal =
                typeOrdinalForKey(carrierBeforeVisit.declaredTypeKey);
            if (!nestedTypeOrdinal.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    carrierBeforeVisit.range,
                    "anonymous-member carrier has no synthesized aggregate type");
                continue;
            }
            visit(*nestedTypeOrdinal);

            std::vector<std::size_t> candidates = membersByType_[*nestedTypeOrdinal];
            candidates.insert(
                candidates.end(),
                promotedMembersByType_[*nestedTypeOrdinal].begin(),
                promotedMembersByType_[*nestedTypeOrdinal].end());
            for (const std::size_t sourceOrdinal : candidates)
            {
                if (sourceOrdinal >= members_.size()) continue;
                const ClassMemberSymbol source = members_[sourceOrdinal];
                if (source.isCompilerGenerated && !source.isAnonymousMemberPromotion) continue;
                if (source.kind != ClassMemberKind::Field || source.isStatic)
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        source.range,
                        "an unnamed anonymous aggregate can promote instance fields only");
                    continue;
                }

                ClassMemberSymbol alias = source;
                alias.accessibility = intersectAccessibility(
                    members_[carrierOrdinal].accessibility, source.accessibility);
                alias.declaringTypeOrdinal = typeOrdinal;
                alias.partOrdinal = members_[carrierOrdinal].partOrdinal;
                alias.declarationOrdinal = members_[carrierOrdinal].declarationOrdinal;
                alias.syntax = nullptr;
                alias.isCompilerGenerated = true;
                alias.isAnonymousMemberPromotion = true;
                alias.promotedSourceMemberOrdinal = source.isAnonymousMemberPromotion
                    ? source.promotedSourceMemberOrdinal
                    : std::optional<std::size_t>(sourceOrdinal);
                alias.anonymousCarrierMemberOrdinals.clear();
                alias.anonymousCarrierMemberOrdinals.push_back(carrierOrdinal);
                alias.anonymousCarrierMemberOrdinals.insert(
                    alias.anonymousCarrierMemberOrdinals.end(),
                    source.anonymousCarrierMemberOrdinals.begin(),
                    source.anonymousCarrierMemberOrdinals.end());
                alias.explicitFieldOffset.reset();
                alias.explicitFieldAlignment.reset();
                alias.declaringTypeIsStruct =
                    typeOrdinal < nameLookup.namedTypes().size() &&
                    nameLookup.namedTypes()[typeOrdinal].kind == NamedTypeDeclarationKind::Struct;

                const auto direct = directNames.find(alias.name);
                const auto previousPromotion = promotedNames.find(alias.name);
                if (direct != directNames.end() || previousPromotion != promotedNames.end())
                {
                    const std::size_t previousOrdinal = direct != directNames.end()
                        ? direct->second : previousPromotion->second;
                    if (previousOrdinal < members_.size())
                    {
                        reportDuplicate(
                            alias,
                            members_[previousOrdinal],
                            diagnostics,
                            "promoted anonymous member name '" + alias.name +
                                "' conflicts in type '" +
                                nameLookup.namedTypes()[typeOrdinal].identity.fullyQualifiedName() + "'");
                    }
                    continue;
                }

                const std::size_t aliasOrdinal = members_.size();
                members_.push_back(std::move(alias));
                promotedMembersByType_[typeOrdinal].push_back(aliasOrdinal);
                promotedNames.emplace(members_[aliasOrdinal].name, aliasOrdinal);
            }
        }
        states[typeOrdinal] = VisitState::Complete;
    };

    for (std::size_t typeOrdinal = 0; typeOrdinal < nameLookup.namedTypes().size(); ++typeOrdinal)
    {
        visit(typeOrdinal);
    }
    return !diagnostics.hasErrors();
}

// Stage 51.x: enum member support in the OOP pipeline. Each `EnumMemberDeclaration` becomes a
// `public static const int` member of its enum type, reusing the existing Stage 37 native
// constant-field machinery for the read side (semantic member access, HIR/MIR field-initializer
// materialization, and LLVM constant-global lowering) end to end -- no new member kind and no
// new LLVM lowering path are introduced. Values follow the same rule as the classic single-file
// enum pipeline in SemanticAnalyzer::analyzeEnum: implicit members auto-increment from 0, and an
// explicit initializer is only accepted when it is a plain (optionally negated) integer literal;
// this is narrower than a full constant-expression evaluator, but it covers every enum this
// project declares (StringComparison, StringSplitOptions) and it fails loudly instead of
// silently mis-evaluating something more exotic.
namespace
{
// EL VALOR DE UN ENUMERADOR SE LLEVA COMO (MAGNITUD SIN SIGNO, SIGNO), NO COMO UN `int64_t`.
//
// Parece un rodeo y no lo es: los dos extremos del rango que un enum puede declarar --el minimo de
// `long`, que es -2^63, y el maximo de `ulong`, que es 2^64-1-- NO CABEN EN EL MISMO TIPO DE C++.
// Estrechar a `int64_t` antes de comprobar hace que `ulong.MaxValue` se vea como -1 y se rechace
// una constante perfectamente legal, y hace que el limite superior de `ulong` sea incomprobable.
// Con el par, el rango de los diez tipos enteros se comprueba exactamente y sin casos especiales.
struct EnumValue
{
    std::uint64_t magnitude = 0;
    bool negative = false;
};

// ¿Cabe este valor en el tipo subyacente? Se compara sobre el ancho y el signo REALES.
//
// LOS DOS LADOS IMPORTAN. Demasiado ancho aceptaria 300 en un `enum : byte` y lo truncaria al
// materializarlo -- un valor que el programa nunca escribio. Demasiado estrecho no dejaria escribir
// en un `enum : long` o `: ulong` constantes que C escribe todos los dias.
bool enumValueFits(EnumValue value, const builtins::IntegerTypeInfo* underlying)
{
    if (underlying == nullptr || underlying->width == 0 || underlying->width > 64) return false;
    const unsigned width = underlying->width;
    if (value.negative)
    {
        // `-0` es 0, y es el unico negativo que un tipo sin signo admite.
        if (!underlying->isSigned) return value.magnitude == 0;
        // El minimo de un tipo con signo de N bits es -2^(N-1): la magnitud llega justo a 2^(N-1).
        const std::uint64_t limit =
            width >= 64U ? (std::uint64_t{1} << 63) : (std::uint64_t{1} << (width - 1U));
        return value.magnitude <= limit;
    }
    if (underlying->isSigned)
    {
        const std::uint64_t limit =
            (width >= 64U ? (std::uint64_t{1} << 63) : (std::uint64_t{1} << (width - 1U))) - 1U;
        return value.magnitude <= limit;
    }
    if (width >= 64U) return true;   // 2^64-1 no se puede escribir como (1<<64)-1
    return value.magnitude <= (std::uint64_t{1} << width) - 1U;
}

// El patron de bits que se guarda. Los consumidores del valor --el evaluador de constantes, el
// HIR y el backend-- lo convierten a `uint64_t` inmediatamente y lo normalizan por el tipo, asi
// que lo que el `int64_t` de ClassMemberSymbol lleva son BITS y no una magnitud con signo. Por eso
// `ulong.MaxValue` puede viajar dentro de el.
std::uint64_t enumValueBits(EnumValue value)
{
    return value.negative ? (0U - value.magnitude) : value.magnitude;
}

// El sucesor, para el auto-incremento. Se calcula sobre el par y no sobre los bits, para que
// -1 -> 0 -> 1 sea una cuenta y no una envoltura.
//
// `hasValue` en falso significa NO HAY SUCESOR REPRESENTABLE: el anterior era 2^64-1, que es el
// mayor entero que cualquier tipo subyacente admite. Hace falta un tercer estado y no vale saturar
// devolviendo el mismo valor -- eso es lo que hacia la primera version de esta funcion, y el
// resultado era que
//
//     enum E : ulong { Max = 18446744073709551615, Siguiente }
//
// compilaba y `Siguiente` valia LO MISMO que `Max`, en silencio. Saturar es una RESPUESTA FALSA:
// el programa recibe un valor que nadie escribio y dos nombres distintos que no se pueden
// distinguir. Y el comentario que lo acompanaba era ademas incorrecto -- decia que `enumValueFits`
// lo rechazaria, cuando 2^64-1 cabe perfectamente en `ulong` y por eso pasaba.
struct EnumSuccessor
{
    EnumValue value;
    bool hasValue = true;
};

EnumSuccessor enumValueNext(EnumValue value)
{
    if (value.negative && value.magnitude > 0)
    {
        const std::uint64_t reduced = value.magnitude - 1U;
        return EnumSuccessor{EnumValue{reduced, reduced != 0}, true};
    }
    if (value.magnitude == ~std::uint64_t{0}) return EnumSuccessor{value, false};
    return EnumSuccessor{EnumValue{value.magnitude + 1U, false}, true};
}

// Enum members are registered before method-body name binding runs, but their initializers need
// the same typed integer constant evaluator used by const fields and switch labels. This adapter
// deliberately resolves only names already registered in the enum being collected. That permits
// the normal left-to-right idiom (`All = Red | Green | Blue | Alpha`) without making a dynamic
// expression, a member from another type, or a forward reference look constant by accident.
class EnumInitializerResolver final : public ProjectConstantMemberResolver
{
public:
    EnumInitializerResolver(
        const ProjectMemberModel& members,
        const ProjectNameLookup& names) noexcept
        : members_(members), names_(names)
    {
    }

    std::optional<std::size_t> resolveConstantMember(
        const ast::AstNode* syntax,
        std::optional<std::size_t> declaringTypeOrdinal) const override
    {
        if (syntax == nullptr || !declaringTypeOrdinal.has_value() ||
            syntax->kind() != ast::AstNodeKind::IdentifierExpression)
        {
            return std::nullopt;
        }

        const std::span<const std::size_t> candidates =
            members_.findMembers(*declaringTypeOrdinal, syntax->text());
        if (candidates.size() != 1) return std::nullopt;
        return candidates.front();
    }

    bool describeConstantMember(
        std::size_t memberOrdinal,
        bool& isEnumConstant,
        std::int64_t& enumConstantValue,
        std::string& enumUnderlyingTypeKey,
        const ast::AstNode*& initializer,
        std::string& declaredTypeKey,
        std::optional<std::size_t>& declaringTypeOrdinal) const override
    {
        if (memberOrdinal >= members_.members().size()) return false;
        const ClassMemberSymbol& member = members_.members()[memberOrdinal];
        if (!member.isEnumConstant || member.declaringTypeOrdinal >= names_.namedTypes().size())
        {
            return false;
        }

        isEnumConstant = true;
        enumConstantValue = member.enumConstantValue;
        enumUnderlyingTypeKey = std::string(
            ::nativesharp::frontend::enumUnderlyingTypeKey(
                names_.namedTypes()[member.declaringTypeOrdinal].syntax));
        initializer = nullptr;
        declaredTypeKey = member.declaredTypeKey;
        declaringTypeOrdinal = member.declaringTypeOrdinal;
        return true;
    }

    std::string resolveTypeKey(const ast::TypeSyntax* typeSyntax) const override
    {
        return members_.canonicalTypeKey(typeSyntax, names_);
    }

private:
    const ProjectMemberModel& members_;
    const ProjectNameLookup& names_;
};

EnumValue enumValueFromIntegerConstant(const ProjectIntegerValue& value)
{
    if (ProjectIntegerEvaluator::isSignedInteger(value.typeKey) && value.asSigned() < 0)
    {
        return EnumValue{std::uint64_t{0} - value.bits, true};
    }
    return EnumValue{value.bits, false};
}
}

bool ProjectMemberModel::collectEnumMembers(
    std::size_t typeOrdinal,
    const NamedTypeDeclaration& type,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (type.syntax == nullptr)
    {
        return !diagnostics.hasErrors();
    }

    // EL RANGO ADMITIDO SALE DEL TIPO SUBYACENTE, no siempre de `int`.
    //
    // Antes esto exigia que todo valor cupiera en un `int` de 32 bits. Con `enum : byte` eso seria
    // demasiado ancho --aceptaria 300 en un enum de un byte y lo truncaria al materializarlo-- y
    // con `enum : long` demasiado estrecho. Las dos direcciones son fallos, y la segunda ni
    // siquiera dejaria escribir constantes que C escribe todos los dias.
    //
    // La regla del tipo subyacente esta en include/nativesharp/Frontend/EnumUnderlyingType.h, que
    // es la misma que consultan el modelo de layout y el backend.
    const std::string_view underlyingKey = enumUnderlyingTypeKey(type.syntax);
    const builtins::IntegerTypeInfo* underlying = builtins::findIntegerType(underlyingKey);
    // `enum E : bool` o `enum E : MiClase` se rechaza aqui, una vez, y no en cada miembro.
    if (type.syntax->typeSyntax() != nullptr && declaredEnumUnderlyingTypeKey(type.syntax).empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidEnumInitializer,
            diagnostics::DiagnosticSeverity::Error,
            type.range,
            "the underlying type of an enum must be a built-in integer type "
            "(byte, sbyte, short, ushort, int, uint, long, ulong, nint, nuint)");
        return false;
    }

    std::unordered_map<std::string, const ast::AstNode*> seen;
    const EnumInitializerResolver initializerResolver(*this, *nameLookup_);
    const ProjectIntegerEvaluator integerEvaluator(&initializerResolver);
    EnumSuccessor nextValue;
    for (const ast::AstNode* memberNode : type.syntax->children())
    {
        if (memberNode == nullptr || memberNode->kind() != ast::AstNodeKind::EnumMemberDeclaration)
        {
            continue;
        }

        const auto previous = seen.find(memberNode->text());
        if (previous != seen.end())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                memberNode->range(),
                "duplicate enum member '" + memberNode->text() + "' in type '" +
                    type.identity.fullyQualifiedName() + "'");
            diagnostics.report(
                diagnostics::DiagnosticId::PreviousDeclaration,
                diagnostics::DiagnosticSeverity::Note,
                previous->second->range(),
                "previous declaration is here");
            continue;
        }
        seen.emplace(memberNode->text(), memberNode);

        // SIN SUCESOR NO HAY MIEMBRO. Un enumerador sin inicializador detras del maximo de 64 bits
        // no tiene ningun valor que tomar, y darle uno --el que sea-- seria inventarlo.
        if (!nextValue.hasValue && memberNode->first() == nullptr)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidEnumInitializer,
                diagnostics::DiagnosticSeverity::Error,
                memberNode->range(),
                "enum member '" + memberNode->text() + "' has no value: the previous member is the "
                "largest value of the underlying type '" + std::string(underlyingKey) +
                "', so there is no next one to count to");
            continue;
        }

        EnumValue value = nextValue.value;
        if (memberNode->first() != nullptr)
        {
            const ast::AstNode* initializer = memberNode->first();
            const std::optional<ProjectIntegerValue> folded =
                integerEvaluator.evaluate(initializer, typeOrdinal);
            if (!folded.has_value())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidEnumInitializer,
                    diagnostics::DiagnosticSeverity::Error,
                    memberNode->range(),
                    "enum initializer for '" + memberNode->text() +
                        "' must be an integral constant expression");
                continue;
            }
            value = enumValueFromIntegerConstant(*folded);
        }
        // LA COMPROBACION DE RANGO ESTA FUERA DEL `if` A PROPOSITO. El valor de un miembro sin
        // inicializador sale del auto-incremento, y ese tambien se puede salir: en
        // `enum E : byte { A = 255, B }` el 255 cabe y el 256 no, y quien lo escribio no puso
        // ningun numero que mirar. Comprobar solo lo escrito dejaria pasar justo el caso que nadie
        // ve.
        if (!enumValueFits(value, underlying))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidEnumInitializer,
                diagnostics::DiagnosticSeverity::Error,
                memberNode->range(),
                "enum value for '" + memberNode->text() + "' does not fit in the underlying "
                "type '" + std::string(underlyingKey) + "' of this enum");
            continue;
        }
        nextValue = enumValueNext(value);

        ClassMemberSymbol symbol;
        symbol.kind = ClassMemberKind::Field;
        symbol.accessibility = ClassMemberAccessibility::Public;
        symbol.declaringTypeOrdinal = typeOrdinal;
        symbol.fileId = type.fileId;
        symbol.range = memberNode->range();
        symbol.syntax = memberNode;
        symbol.sourcePath = type.parts.empty() ? std::string{} : type.parts.front().sourcePath;
        symbol.isPrecompiled = type.isPrecompiled;
        symbol.name = memberNode->text();
        // The member's declared/visible type is the enum itself (so `Mode.Second` type-checks as
        // `Mode`, matching a `Mode`-typed parameter/local and comparing correctly against other
        // `Mode` values) -- not its int-sized underlying storage. `isEnumConstant` below is what
        // tells the MIR/LLVM layers to still treat it as an integral compile-time constant.
        symbol.declaredTypeKey = "type:" + type.identity.key();
        symbol.isStatic = true;
        symbol.isReadonly = true;
        symbol.isConst = true;
        symbol.isEnumConstant = true;
        // BITS, no magnitud con signo -- ver enumValueBits(). Es lo que deja que un
        // `enum E : ulong { Max = 18446744073709551615 }` viaje en un `int64_t`.
        symbol.enumConstantValue = static_cast<std::int64_t>(enumValueBits(value));

        const std::size_t memberOrdinal = members_.size();
        members_.push_back(std::move(symbol));
        membersByType_[typeOrdinal].push_back(memberOrdinal);
        membersByName_[typeOrdinal][members_[memberOrdinal].name].push_back(memberOrdinal);
        memberBySyntax_.emplace(memberNode, memberOrdinal);
    }

    return !diagnostics.hasErrors();
}

void ProjectMemberModel::scanMethodBody(
    const ast::AstNode* node,
    std::size_t typeOrdinal,
    std::size_t methodMemberOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (node == nullptr)
    {
        return;
    }

    if (node->kind() == ast::AstNodeKind::ThisExpression)
    {
        bodyBindings_.push_back(MemberBodyBinding{
            MemberBodyBindingKind::ThisReference,
            node,
            typeOrdinal,
            methodMemberOrdinal,
            {},
            {}});
        return;
    }

    if (node->kind() == ast::AstNodeKind::MemberExpression &&
        node->first() != nullptr &&
        node->first()->kind() == ast::AstNodeKind::ThisExpression)
    {
        scanMethodBody(node->first(), typeOrdinal, methodMemberOrdinal, diagnostics);
        const std::span<const std::size_t> directCandidates = findMembers(typeOrdinal, node->text());
        std::vector<std::size_t> candidates(directCandidates.begin(), directCandidates.end());
        if (candidates.empty())
        {
            for (const std::size_t ordinal : findMembers(typeOrdinal, "get_" + node->text()))
            {
                if (ordinal < members_.size() && members_[ordinal].isPropertyAccessor &&
                    members_[ordinal].isGetter)
                {
                    candidates.push_back(ordinal);
                }
            }
        }
        if (candidates.empty())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::UnknownMember,
                diagnostics::DiagnosticSeverity::Error,
                node->range(),
                "class has no instance member named '" + node->text() + "'");
        }
        bodyBindings_.push_back(MemberBodyBinding{
            MemberBodyBindingKind::ThisMemberAccess,
            node,
            typeOrdinal,
            methodMemberOrdinal,
            node->text(),
            std::move(candidates)});
        return;
    }

    // IDs 80/83. `this->x`. Este rechazo estaba escrito contra PointerMemberExpression, el nodo
    // propio que el parser construia para `->` antes de que `p->x` pasara a construirse como
    // `(*p).x`. Al desaparecer el nodo, la comprobacion quedo muerta y `this->Value` empezo a
    // COMPILAR -- lo vio `nsc.project-member-model`, que es la unica prueba que mide esta fase.
    //
    // El mensaje se conserva palabra por palabra, y con el su DiagnosticId, porque es mejor que el
    // generico: no dice "no es un puntero", dice QUE ESCRIBIR en su lugar. Lo unico que cambia es
    // la forma que se reconoce: un acceso a miembro sobre el `*` SINTETIZADO que el parser marca
    // (auxiliaryText "arrow"), sobre `this`.
    if (node->kind() == ast::AstNodeKind::MemberExpression &&
        node->first() != nullptr &&
        node->first()->kind() == ast::AstNodeKind::UnaryExpression &&
        node->first()->auxiliaryText() == "arrow" &&
        node->first()->first() != nullptr &&
        node->first()->first()->kind() == ast::AstNodeKind::ThisExpression)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidMemberAccess,
            diagnostics::DiagnosticSeverity::Error,
            node->range(),
            "'this' is a class reference; use '.' rather than '->' for member lookup");
        scanMethodBody(node->first()->first(), typeOrdinal, methodMemberOrdinal, diagnostics);
        return;
    }

    scanMethodBody(node->first(), typeOrdinal, methodMemberOrdinal, diagnostics);
    scanMethodBody(node->second(), typeOrdinal, methodMemberOrdinal, diagnostics);
    scanMethodBody(node->third(), typeOrdinal, methodMemberOrdinal, diagnostics);
    for (const ast::AstNode* child : node->children())
    {
        scanMethodBody(child, typeOrdinal, methodMemberOrdinal, diagnostics);
    }
}

void ProjectMemberModel::scanInvalidThisContexts(
    const ast::AstNode* node,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (node == nullptr)
    {
        return;
    }

    if (node->kind() == ast::AstNodeKind::MethodDeclaration ||
        node->kind() == ast::AstNodeKind::ConstructorDeclaration ||
        node->kind() == ast::AstNodeKind::DestructorDeclaration)
    {
        return;
    }

    if (node->kind() == ast::AstNodeKind::ThisExpression)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidMemberAccess,
            diagnostics::DiagnosticSeverity::Error,
            node->range(),
            "'this' is available only inside an instance method");
        return;
    }

    scanInvalidThisContexts(node->first(), diagnostics);
    scanInvalidThisContexts(node->second(), diagnostics);
    scanInvalidThisContexts(node->third(), diagnostics);
    for (const ast::AstNode* child : node->children())
    {
        scanInvalidThisContexts(child, diagnostics);
    }
}

void ProjectMemberModel::reportDuplicate(
    const ClassMemberSymbol& current,
    const ClassMemberSymbol& previous,
    diagnostics::DiagnosticEngine& diagnostics,
    std::string message) const
{
    diagnostics.report(
        diagnostics::DiagnosticId::DuplicateSymbol,
        diagnostics::DiagnosticSeverity::Error,
        current.range,
        std::move(message));
    diagnostics.report(
        diagnostics::DiagnosticId::PreviousDeclaration,
        diagnostics::DiagnosticSeverity::Note,
        previous.range,
        "previous member declaration is here");
}

const char* classMemberKindName(ClassMemberKind kind) noexcept
{
    switch (kind)
    {
        case ClassMemberKind::Field:
            return "field";
        case ClassMemberKind::Event:
            return "event";
        case ClassMemberKind::Method:
            return "method";
        case ClassMemberKind::Constructor:
            return "constructor";
        case ClassMemberKind::Destructor:
            return "destructor";
    }
    return "field";
}

const char* classMemberAccessibilityName(ClassMemberAccessibility accessibility) noexcept
{
    switch (accessibility)
    {
        case ClassMemberAccessibility::Private:
            return "private";
        case ClassMemberAccessibility::Protected:
            return "protected";
        case ClassMemberAccessibility::Internal:
            return "internal";
        case ClassMemberAccessibility::Public:
            return "public";
    }
    return "private";
}

const char* memberBodyBindingKindName(MemberBodyBindingKind kind) noexcept
{
    switch (kind)
    {
        case MemberBodyBindingKind::ThisReference:
            return "this";
        case MemberBodyBindingKind::ThisMemberAccess:
            return "this-member";
    }
    return "this";
}
}
