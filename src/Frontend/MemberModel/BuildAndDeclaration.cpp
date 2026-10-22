#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "../Internal/EnumInitializerResolver.h"
#include "../Internal/AnonymousMemberPromotionVisitState.h"
#include "../Internal/EnumValue.h"
#include "../Internal/EnumSuccessor.h"

#include "nativesharp/Language/AggregateSemantics.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Language/TraitSemantics.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/CoroutineSemantics.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Compilation/ProjectGraph.h"

#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/PrecompiledMemberIdentityIndex.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <filesystem>
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
std::string modulePathKey(const std::filesystem::path& path)
{
    std::string key = path.lexically_normal().generic_string();
    std::ranges::transform(key, key.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return key;
}

language::PointerContract pointerContractFromSyntax(const ast::TypeSyntax* syntax) noexcept
{
    language::PointerContract contract;
    if (syntax == nullptr) return contract;
    contract.mutablePointee = !syntax->isPointeeConst();
    for (auto modifier = syntax->modifiers().rbegin(); modifier != syntax->modifiers().rend(); ++modifier)
    {
        if (modifier->kind != ast::TypeModifierSyntaxKind::Pointer) continue;
        contract.nullable = modifier->pointerNullability !=
            ast::PointerNullabilitySyntaxKind::NonNull;
        contract.hasProvenance = modifier->pointerHasProvenance;
        contract.knownAlignment = modifier->pointerKnownAlignment;
        contract.addressSpace = modifier->pointerAddressSpace;
        break;
    }
    return contract;
}

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
    const std::optional<std::size_t> ordinal =
        nameLookup.typeOrdinalForIdentity(typeKey.substr(5));
    return ordinal.has_value() ? nameLookup.typeForOrdinal(*ordinal) : nullptr;
}

// SHR-013 "vistas y tiempo de vida": el tipo se declaro `borrowed struct`. Lee la MISMA marca
// que el parser escribio en el texto auxiliar de la declaracion y que ProjectMethodBodyModel
// vuelve a leer -- una marca, dos lectores, cero copias de la regla.
bool namedTypeIsBorrowedView(const NamedTypeDeclaration* type) noexcept
{
    return type != nullptr && type->syntax != nullptr &&
        type->syntax->auxiliaryText().find("borrowedview") != std::string::npos;
}

bool declaresLifetime(const ast::AstNode* declaration, std::string_view name) noexcept
{
    return declaration != nullptr && std::ranges::any_of(
        declaration->lifetimeParameters(),
        [&](const ast::LifetimeParameterSyntax& parameter)
        {
            return parameter.name == name;
        });
}

ParameterPassKind lifetimeParameterPassKind(const ast::AstNode& parameter) noexcept
{
    if (parameter.auxiliaryText().starts_with("pass:ref")) return ParameterPassKind::Ref;
    if (parameter.auxiliaryText().starts_with("pass:out")) return ParameterPassKind::Out;
    if (parameter.auxiliaryText().starts_with("pass:in")) return ParameterPassKind::In;
    return ParameterPassKind::Value;
}

std::optional<std::string> parameterContract(
    const ast::AstNode& parameter,
    std::string_view prefix)
{
    std::string_view markers = parameter.auxiliaryText();
    std::size_t begin = 0;
    while (begin <= markers.size())
    {
        const std::size_t end = markers.find('|', begin);
        const std::string_view marker = markers.substr(
            begin, end == std::string_view::npos ? markers.size() - begin : end - begin);
        if (marker.starts_with(prefix)) return std::string(marker.substr(prefix.size()));
        if (end == std::string_view::npos) break;
        begin = end + 1U;
    }
    return std::nullopt;
}

bool lifetimeOutlives(
    std::span<const std::pair<std::string, std::string>> constraints,
    std::string_view longer,
    std::string_view shorter)
{
    if (longer == shorter || longer == "static") return true;
    std::unordered_set<std::string> visited;
    std::vector<std::string> pending{std::string(longer)};
    while (!pending.empty())
    {
        std::string current = std::move(pending.back());
        pending.pop_back();
        if (!visited.insert(current).second) continue;
        for (const auto& [declaredLonger, declaredShorter] : constraints)
        {
            if (declaredLonger != current) continue;
            if (declaredShorter == shorter) return true;
            pending.push_back(declaredShorter);
        }
    }
    return false;
}

std::optional<std::pair<std::string, std::string>> lifetimeCycle(
    std::span<const std::pair<std::string, std::string>> constraints)
{
    for (const auto& [longer, shorter] : constraints)
        if (longer == shorter || lifetimeOutlives(constraints, shorter, longer))
            return std::pair{longer, shorter};
    return std::nullopt;
}

std::unordered_set<std::string> collectDelegateInputLifetimes(
    const NamedTypeDeclaration& type,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::unordered_set<std::string> inputLifetimes;
    std::size_t parameterOrdinal = 0;
    for (const ast::AstNode* parameter : type.syntax->children())
    {
        if (parameter == nullptr || parameter->kind() != ast::AstNodeKind::ParameterDeclaration)
            continue;
        const ParameterPassKind passKind = lifetimeParameterPassKind(*parameter);
        std::string lifetime = parameter->lifetimeName();
        if (!lifetime.empty())
        {
            if (passKind == ParameterPassKind::Out)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    parameter->range(),
                    "out delegate parameter '" + parameter->text() +
                        "' initializes storage and cannot carry an input lifetime");
            }
            else if (passKind == ParameterPassKind::Value)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    parameter->range(),
                    "only ref or in delegate parameters may carry a named lifetime");
            }
            else if (!declaresLifetime(type.syntax, lifetime))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::UnknownIdentifier,
                    diagnostics::DiagnosticSeverity::Error,
                    parameter->range(),
                    "delegate parameter '" + parameter->text() +
                        "' names undeclared lifetime '" + lifetime + "'");
            }
        }
        else if (passKind == ParameterPassKind::Ref || passKind == ParameterPassKind::In)
        {
            lifetime = "$p" + std::to_string(parameterOrdinal);
        }
        if (!lifetime.empty() && passKind != ParameterPassKind::Out)
            inputLifetimes.insert(std::move(lifetime));
        ++parameterOrdinal;
    }
    return inputLifetimes;
}

bool hasOutlivingDelegateInput(
    const std::vector<std::pair<std::string, std::string>>& constraints,
    const std::unordered_set<std::string>& inputLifetimes,
    std::string_view resultLifetime)
{
    for (const std::string& input : inputLifetimes)
        if (lifetimeOutlives(constraints, input, resultLifetime)) return true;
    return false;
}

void validateDelegateResultLifetime(
    const NamedTypeDeclaration& type,
    const std::vector<std::pair<std::string, std::string>>& constraints,
    const std::unordered_set<std::string>& inputLifetimes,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!type.syntax->isReference()) return;
    const std::string& resultLifetime = type.syntax->lifetimeName();
    if (resultLifetime.empty() && inputLifetimes.size() > 1U)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidReturn,
            diagnostics::DiagnosticSeverity::Error,
            type.syntax->range(),
            "ref-returning delegate '" + type.identity.name() +
                "' has multiple input lifetimes; write ref[a] and bind the intended "
                "ref/in parameters to lifetime a");
    }
    else if (resultLifetime == "this")
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidReturn,
            diagnostics::DiagnosticSeverity::Error,
            type.syntax->range(),
            "lifetime 'this' is unavailable on a delegate");
    }
    else if (!resultLifetime.empty() && resultLifetime != "static" &&
             !declaresLifetime(type.syntax, resultLifetime))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::UnknownIdentifier,
            diagnostics::DiagnosticSeverity::Error,
            type.syntax->range(),
            "ref return of delegate '" + type.identity.name() +
                "' names undeclared lifetime '" + resultLifetime + "'");
    }
    else if (!resultLifetime.empty() && resultLifetime != "static" &&
             !hasOutlivingDelegateInput(constraints, inputLifetimes, resultLifetime))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidReturn,
            diagnostics::DiagnosticSeverity::Error,
            type.syntax->range(),
            "delegate ref return lifetime '" + resultLifetime +
                "' has no ref/in parameter whose lifetime outlives it");
    }
}

void validateDelegateLifetimeContract(
    const NamedTypeDeclaration& type,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (type.syntax == nullptr) return;
    std::vector<std::pair<std::string, std::string>> constraints;
    for (const ast::LifetimeOutlivesConstraint& constraint :
         type.syntax->lifetimeOutlivesConstraints())
        constraints.emplace_back(constraint.longer, constraint.shorter);
    if (const auto cycle = lifetimeCycle(constraints))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            type.syntax->range(),
            "lifetime constraints contain a cycle between '" + cycle->first +
                "' and '" + cycle->second + "'");
    }
    const std::unordered_set<std::string> inputLifetimes =
        collectDelegateInputLifetimes(type, diagnostics);
    validateDelegateResultLifetime(type, constraints, inputLifetimes, diagnostics);
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
    if (const std::optional<language::FunctionType> functionType =
            language::parseFunctionTypeIdentity(typeKey))
    {
        if (functionType->carrier != language::FunctionCarrier::Pointer ||
            functionType->callingConvention == language::CallingConvention::NativeSharp ||
            functionType->isAsync || !functionType->errorType.empty())
            return false;
        if (!isStage47CAbiType(functionType->result, nameLookup, true)) return false;
        return std::ranges::all_of(
            functionType->parameters,
            [&](const std::string& parameter)
            { return isStage47CAbiType(parameter, nameLookup, false); });
    }
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
        typeKey == "builtin:float" || typeKey == "builtin:double" ||
        // NS304-T15-F03: decimal crosses the C ABI as the DECLARED 128-bit carrier (NS_Decimal),
        // not an assumed externally-compatible layout.
        typeKey == "builtin:decimal")
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
        modifiers.front().kind == ast::TypeModifierSyntaxKind::RangeArray ||
        modifiers.front().kind == ast::TypeModifierSyntaxKind::FlexibleArray;
}

bool isTraitDeclaration(const NamedTypeDeclaration& type)
{
    return std::ranges::any_of(type.parts, [](const NamedTypeDeclarationPart& part)
    {
        if (part.syntax == nullptr) return false;
        const std::string_view metadata = part.syntax->auxiliaryText();
        return metadata == "ns3-trait" ||
            metadata.find("nsc-meta:ns3-trait") != std::string_view::npos;
    });
}
}


void ProjectMemberModel::validateDelegateParameters(
    const NamedTypeDeclaration& type,
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (type.syntax == nullptr) return;
    const std::string declaredErrorTypeKey = type.syntax->throwsTypeSyntax() == nullptr
        ? std::string{}
        : canonicalTypeKey(type.syntax->throwsTypeSyntax(), nameLookup);
    if (type.syntax->hasModifier(lexer::TokenKind::KwExtern) &&
        !declaredErrorTypeKey.empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidExternCSignature,
            diagnostics::DiagnosticSeverity::Error,
            type.range,
            "extern C/vectorcall delegates cannot declare throws; use a non-throwing adapter at the ABI boundary");
    }
    std::unordered_map<std::string, const ast::AstNode*> parameterNames;
    for (const ast::AstNode* parameter : type.syntax->children())
    {
        if (parameter == nullptr ||
            parameter->kind() != ast::AstNodeKind::ParameterDeclaration)
            continue;
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
        else parameterNames.emplace(parameter->text(), parameter);
    }
}

void ProjectMemberModel::validateExternDelegate(
    const NamedTypeDeclaration& type,
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (type.syntax == nullptr ||
        !type.syntax->hasModifier(lexer::TokenKind::KwExtern))
        return;
    if (!type.syntax->genericParameters().empty())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidExternCSignature,
            diagnostics::DiagnosticSeverity::Error,
            type.range,
            "extern C delegate types cannot be generic in Stage 47");
    }
    const std::string returnType = canonicalTypeKey(
        type.syntax->typeSyntax(), nameLookup);
    if (!isStage47CAbiType(returnType, nameLookup, true))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidExternCSignature,
            diagnostics::DiagnosticSeverity::Error,
            type.range,
            "extern C delegate return type '" + returnType +
                "' is not C ABI compatible");
    }
    for (const ast::AstNode* parameter : type.syntax->children())
    {
        if (parameter == nullptr ||
            parameter->kind() != ast::AstNodeKind::ParameterDeclaration)
            continue;
        const std::string parameterType = canonicalTypeKey(
            parameter->typeSyntax(), nameLookup);
        if (!isStage47CAbiType(parameterType, nameLookup, false))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidExternCSignature,
                diagnostics::DiagnosticSeverity::Error,
                parameter->range(),
                "extern C delegate parameter type '" + parameterType +
                    "' is not C ABI compatible");
        }
    }
}

void ProjectMemberModel::validateDelegateType(
    const NamedTypeDeclaration& type,
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    validateDelegateLifetimeContract(type, diagnostics);
    validateDelegateParameters(type, nameLookup, diagnostics);
    validateExternDelegate(type, nameLookup, diagnostics);
}

void ProjectMemberModel::validateCAbiExportNames(
    diagnostics::DiagnosticEngine& diagnostics) const
{
    std::unordered_map<std::string, const ClassMemberSymbol*> cAbiExports;
    for (const ClassMemberSymbol& member : members_)
    {
        if (!member.isExportedC) continue;
        const auto [iterator, inserted] = cAbiExports.emplace(member.cAbiName, &member);
        if (inserted) continue;
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

void ProjectMemberModel::collectImportedCLibraries(
    std::unordered_map<
        std::string,
        std::pair<const ClassMemberSymbol*, std::string>>& importsByLibraryAndSymbol,
    std::unordered_map<std::string, std::vector<std::string>>& librariesBySymbol,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    for (const ClassMemberSymbol& member : members_)
    {
        if (!member.isImportedC) continue;
        const std::string library = upperAsciiText(member.importDllName);
        const std::string signature = importedCSignature(member);
        const auto [iterator, inserted] = importsByLibraryAndSymbol.emplace(
            library + "!" + member.name,
            std::make_pair(&member, signature));
        if (!inserted && iterator->second.second != signature)
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
            libraries.push_back(library);
    }
}

bool ProjectMemberModel::buildModuleInitializationPlan(
    const compilation::ProjectGraph& graph,
    std::string& error)
{
    error.clear();
    moduleInitializers_.clear();
    if (!built_ || nameLookup_ == nullptr)
    {
        error = "module initialization planning requires a completed member model";
        return false;
    }

    const std::span<const compilation::Project> projects = graph.projects();
    if (projects.size() != nameLookup_->modules().size())
    {
        error = "module initialization planning requires one declaration space per project";
        return false;
    }

    std::unordered_map<std::string, std::size_t> projectOrdinalByPath;
    projectOrdinalByPath.reserve(projects.size());
    for (std::size_t projectOrdinal = 0; projectOrdinal < projects.size(); ++projectOrdinal)
    {
        projectOrdinalByPath.emplace(
            modulePathKey(projects[projectOrdinal].projectFile()), projectOrdinal);
    }

    moduleInitializers_.reserve(projects.size());
    for (std::size_t projectOrdinal = 0; projectOrdinal < projects.size(); ++projectOrdinal)
    {
        ModuleInitializationRecord record;
        record.projectOrdinal = projectOrdinal;
        record.moduleName = projects[projectOrdinal].moduleName();

        for (const std::filesystem::path& reference : projects[projectOrdinal].projectReferences())
        {
            const auto dependency = projectOrdinalByPath.find(modulePathKey(reference));
            if (dependency == projectOrdinalByPath.end())
            {
                error = "module '" + record.moduleName + "' references project '" +
                    reference.generic_string() + "' outside the loaded project graph";
                moduleInitializers_.clear();
                return false;
            }
            record.dependencyProjectOrdinals.push_back(dependency->second);
        }
        std::ranges::sort(record.dependencyProjectOrdinals);
        record.dependencyProjectOrdinals.erase(
            std::unique(
                record.dependencyProjectOrdinals.begin(),
                record.dependencyProjectOrdinals.end()),
            record.dependencyProjectOrdinals.end());

        for (std::size_t typeOrdinal = 0;
             typeOrdinal < nameLookup_->namedTypes().size();
             ++typeOrdinal)
        {
            if (nameLookup_->namedTypes()[typeOrdinal].projectOrdinal != projectOrdinal) continue;
            for (const std::size_t memberOrdinal : membersOfType(typeOrdinal))
            {
                if (memberOrdinal >= members_.size()) continue;
                const ClassMemberSymbol& member = members_[memberOrdinal];
                if (member.kind == ClassMemberKind::Constructor && member.isStatic)
                {
                    record.staticConstructorMemberOrdinals.push_back(memberOrdinal);
                }
            }
        }
        moduleInitializers_.push_back(std::move(record));
    }
    return true;
}


void ProjectMemberModel::validateTypeLifetimeConstraints(
    const NamedTypeDeclaration& type,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (type.syntax == nullptr) return;
    std::vector<std::pair<std::string, std::string>> constraints;
    for (const ast::LifetimeOutlivesConstraint& constraint :
         type.syntax->lifetimeOutlivesConstraints())
        constraints.emplace_back(constraint.longer, constraint.shorter);
    if (const auto cycle = lifetimeCycle(constraints))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            type.syntax->range(),
            "lifetime constraints contain a cycle between '" + cycle->first +
                "' and '" + cycle->second + "'");
    }
}

void ProjectMemberModel::collectExpandedPartMembers(
    const NamedTypeDeclarationPart& part,
    const NamedTypeDeclaration& type,
    std::unordered_map<std::string, const ast::AstNode*>& propertyDeclarations,
    std::unordered_map<std::string, const ast::AstNode*>& ordinaryDeclarations,
    std::vector<const ast::AstNode*>& expandedMembers,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    for (const ast::AstNode* directMember : part.syntax->children())
    {
        const ast::AstNode* declaredMember =
            directMember != nullptr &&
                directMember->kind() == ast::AstNodeKind::StructDeclaration &&
                directMember->auxiliaryText().starts_with("anonymous-") &&
                directMember->first() != nullptr
            ? directMember->first()
            : directMember;
        validateExportedGenericMember(directMember, diagnostics);
        registerTypeMemberDeclaration(
            declaredMember,
            type,
            propertyDeclarations,
            ordinaryDeclarations,
            diagnostics);
        appendExpandedMemberNodes(directMember, expandedMembers);
    }
}

void ProjectMemberModel::collectTypePartMembers(
    std::size_t typeOrdinal,
    const NamedTypeDeclaration& type,
    std::size_t partOrdinal,
    const NamedTypeDeclarationPart& part,
    const ProjectNameLookup& nameLookup,
    std::unordered_map<std::string, std::vector<std::size_t>>& byName,
    std::unordered_map<std::string, const ast::AstNode*>& propertyDeclarations,
    std::unordered_map<std::string, const ast::AstNode*>& ordinaryDeclarations,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (part.syntax == nullptr) return;
    std::size_t declarationOrdinal = 0;
    std::vector<const ast::AstNode*> expandedMembers;
    collectExpandedPartMembers(
        part,
        type,
        propertyDeclarations,
        ordinaryDeclarations,
        expandedMembers,
        diagnostics);
    for (const ast::AstNode* memberNode : expandedMembers)
    {
        if (memberNode == nullptr || !isClassMemberNode(memberNode->kind())) continue;
        collectTypeMember(
            typeOrdinal,
            type,
            partOrdinal,
            part,
            memberNode,
            declarationOrdinal++,
            nameLookup,
            byName,
            diagnostics);
    }
}

void ProjectMemberModel::initializeMemberIdentity(
    ClassMemberSymbol& symbol,
    std::size_t typeOrdinal,
    std::size_t partOrdinal,
    std::size_t declarationOrdinal,
    const NamedTypeDeclarationPart& part,
    const ast::AstNode* memberNode) const
{
    symbol.kind = classMemberKind(memberNode);
    symbol.accessibility = memberAccessibility(memberNode);
    symbol.declaringTypeOrdinal = typeOrdinal;
    symbol.partOrdinal = partOrdinal;
    symbol.sourceOrdinal = part.sourceOrdinal;
    symbol.declarationOrdinal = declarationOrdinal;
    symbol.fileId = part.fileId;
    symbol.range = memberNode->range();
    symbol.syntax = memberNode;
    symbol.sourcePath = part.sourcePath;
}

void ProjectMemberModel::initializeMemberPointerAndOwnership(
    ClassMemberSymbol& symbol,
    const ast::AstNode* memberNode) const
{
    symbol.ffiUnwindPolicy = memberNode->ffiUnwindPolicy();
    symbol.declaredPointerPointeeReadonly = memberNode->typeSyntax() != nullptr &&
        memberNode->typeSyntax()->isPointeeConst();
    const language::PointerContract declaredPointer =
        pointerContractFromSyntax(memberNode->typeSyntax());
    symbol.declaredPointerNullable = declaredPointer.nullable;
    symbol.declaredPointerHasProvenance = declaredPointer.hasProvenance;
    symbol.declaredPointerKnownAlignment = declaredPointer.knownAlignment;
    symbol.declaredPointerAddressSpace = declaredPointer.addressSpace;
    symbol.declaredOwnership =
        (symbol.kind == ClassMemberKind::Constructor ||
         symbol.kind == ClassMemberKind::Destructor)
        ? sema::OwnershipKind::None
        : ownershipKindFromSyntax(memberNode->typeSyntax());
}

void ProjectMemberModel::validateMemberFieldLifetime(
    const ClassMemberSymbol& symbol,
    const NamedTypeDeclaration& type,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (symbol.kind == ClassMemberKind::Field &&
        symbol.declaredOwnership == sema::OwnershipKind::Borrowed &&
        (symbol.declaredTypeSyntax == nullptr ||
         symbol.declaredTypeSyntax->lifetimeName().empty()))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "borrowed field '" + symbol.name +
                "' is rejected in Stage 50 because its lifetime may outlive its source owner");
    }
    if (symbol.kind != ClassMemberKind::Field || symbol.declaredTypeSyntax == nullptr ||
        symbol.declaredTypeSyntax->lifetimeName().empty())
        return;
    const std::string& lifetime = symbol.declaredTypeSyntax->lifetimeName();
    if (!namedTypeIsBorrowedView(&type))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "field '" + symbol.name + "' carries lifetime '" + lifetime +
                "' but its containing struct is not declared 'borrowed struct'");
    }
    else if (!declaresLifetime(type.syntax, lifetime))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::UnknownIdentifier,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "field '" + symbol.name + "' names undeclared lifetime '" + lifetime +
                "' on borrowed view '" + type.identity.name() + "'");
    }
}

void ProjectMemberModel::validateMemberViewField(
    const ClassMemberSymbol& symbol,
    const NamedTypeDeclaration& type,
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics) const
{
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
    if (symbol.kind != ClassMemberKind::Field ||
        !typeKeyIsViewLike(symbol.declaredTypeKey, nameLookup) ||
        namedTypeIsBorrowedView(&type))
        return;
    diagnostics.report(
        diagnostics::DiagnosticId::InvalidType,
        diagnostics::DiagnosticSeverity::Error,
        symbol.range,
        "field '" + symbol.name + "' cannot have view type '" + symbol.declaredTypeKey +
            "': a field outlives every call, and a view borrows from something that "
            "does not. Store the source and build the view where it is used");
}

void ProjectMemberModel::validateVolatileMember(
    const ClassMemberSymbol& symbol,
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics) const
{
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
}

void ProjectMemberModel::initializeMethodParameter(
    MethodParameterSymbol& parameter,
    const ast::AstNode* parameterNode,
    const NamedTypeDeclarationPart& part,
    const ProjectNameLookup& nameLookup) const
{
    parameter.name = parameterNode->text();
    parameter.typeSyntax = parameterNode->typeSyntax();
    parameter.typeKey = canonicalTypeKey(parameterNode->typeSyntax(), nameLookup);
    parameter.pointerPointeeReadonly = parameter.typeSyntax != nullptr &&
        parameter.typeSyntax->isPointeeConst();
    const language::PointerContract parameterPointer =
        pointerContractFromSyntax(parameter.typeSyntax);
    parameter.pointerNullable = parameterPointer.nullable;
    parameter.pointerHasProvenance = parameterPointer.hasProvenance;
    parameter.pointerKnownAlignment = parameterPointer.knownAlignment;
    parameter.pointerAddressSpace = parameterPointer.addressSpace;
    parameter.ownership = ownershipKindFromSyntax(parameterNode->typeSyntax());
    parameter.fileId = part.fileId;
    parameter.range = parameterNode->range();
    parameter.syntax = parameterNode;
    parameter.defaultValueSyntax = parameterNode->first();
    parameter.isParameterPack = parameterNode->auxiliaryText().starts_with("params");
}

void ProjectMemberModel::applyFfiParameterMarkers(
    MethodParameterSymbol& parameter,
    const ast::AstNode* parameterNode) const
{
    if (const auto length = parameterContract(*parameterNode, "ffi:length:");
        length.has_value())
        parameter.ffiLengthParameterName = *length;
    if (const auto callback = parameterContract(*parameterNode, "ffi:callback:");
        callback.has_value())
    {
        parameter.ffiCallback = true;
        parameter.ffiCallbackMayEscape = *callback == "escaping";
    }
}

void ProjectMemberModel::validateParameterLifetime(
    MethodParameterSymbol& parameter,
    const ClassMemberSymbol& symbol,
    const NamedTypeDeclaration& type,
    const ast::AstNode* parameterNode,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!parameterNode->lifetimeName().empty())
    {
        parameter.lifetimeName = parameterNode->lifetimeName();
        if (parameter.passKind == ParameterPassKind::Out)
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                parameter.range,
                "out parameter '" + parameter.name +
                    "' initializes storage and cannot carry an input lifetime");
        else if (parameter.passKind == ParameterPassKind::Value)
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                parameter.range,
                "only ref or in parameters may carry a named lifetime");
        else if (!declaresLifetime(symbol.syntax, parameter.lifetimeName) &&
                 !declaresLifetime(type.syntax, parameter.lifetimeName))
            diagnostics.report(
                diagnostics::DiagnosticId::UnknownIdentifier,
                diagnostics::DiagnosticSeverity::Error,
                parameter.range,
                "parameter '" + parameter.name + "' names undeclared lifetime '" +
                    parameter.lifetimeName + "'");
    }
    else if (parameter.passKind == ParameterPassKind::Ref ||
             parameter.passKind == ParameterPassKind::In)
        parameter.lifetimeName = "$p" + std::to_string(symbol.parameters.size());
}

bool ProjectMemberModel::defaultParameterValueIsCompatible(
    const MethodParameterSymbol& parameter,
    const ast::AstNode* valueSyntax,
    const ProjectNameLookup& nameLookup) const
{
    const ast::AstNodeKind valueKind = valueSyntax->kind();
    if (valueKind == ast::AstNodeKind::IntegerLiteralExpression ||
        valueKind == ast::AstNodeKind::CharacterLiteralExpression)
        return ProjectTypeConversion::isNumeric(parameter.typeKey) &&
            parameter.typeKey != "builtin:bool";
    if (valueKind == ast::AstNodeKind::FloatingLiteralExpression)
        return parameter.typeKey == "builtin:float" ||
            parameter.typeKey == "builtin:double";
    if (valueKind == ast::AstNodeKind::BooleanLiteralExpression)
        return parameter.typeKey == "builtin:bool";
    if (valueKind == ast::AstNodeKind::StringLiteralExpression)
        return parameter.typeKey == "builtin:string" ||
            parameter.typeKey == "builtin:char*";
    if (valueKind != ast::AstNodeKind::NullLiteralExpression) return false;
    const NamedTypeDeclaration* named = namedTypeForKey(parameter.typeKey, nameLookup);
    return parameter.typeKey == "builtin:string" ||
        ProjectTypeConversion::isPointer(parameter.typeKey) ||
        ProjectTypeConversion::isNativeArray(parameter.typeKey) ||
        (named != nullptr &&
         (named->kind == NamedTypeDeclarationKind::Class ||
          named->kind == NamedTypeDeclarationKind::Interface ||
          named->kind == NamedTypeDeclarationKind::Delegate));
}

void ProjectMemberModel::validateCallableParameterIdentity(
    const MethodParameterSymbol& parameter,
    const ClassMemberSymbol& symbol,
    std::unordered_map<std::string, const ast::AstNode*>& parameterNames,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    const std::string callableKind = symbol.kind == ClassMemberKind::Constructor
        ? "constructor" : "method";
    if (isInvalidVoidObjectType(parameter.typeSyntax))
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            parameter.range,
            callableKind + " parameter '" + parameter.name +
                "' cannot have object type 'void'; use a pointer type such as void* when appropriate");
    const auto previousParameter = parameterNames.find(parameter.name);
    if (previousParameter == parameterNames.end())
    {
        parameterNames.emplace(parameter.name, parameter.syntax);
        return;
    }
    diagnostics.report(
        diagnostics::DiagnosticId::DuplicateSymbol,
        diagnostics::DiagnosticSeverity::Error,
        parameter.range,
        "duplicate " + callableKind + " parameter '" + parameter.name +
            "' in member '" + symbol.name + "'");
    diagnostics.report(
        diagnostics::DiagnosticId::PreviousDeclaration,
        diagnostics::DiagnosticSeverity::Note,
        previousParameter->second->range(),
        "previous parameter declaration is here");
}

void ProjectMemberModel::validateFfiParameterContract(
    ClassMemberSymbol& symbol,
    std::size_t parameterOrdinal,
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    MethodParameterSymbol& parameter = symbol.parameters[parameterOrdinal];
    if (!parameter.ffiLengthParameterName.empty())
    {
        const auto target = std::ranges::find_if(
            symbol.parameters,
            [&](const MethodParameterSymbol& candidate)
            {
                return candidate.name == parameter.ffiLengthParameterName;
            });
        if (target == symbol.parameters.end())
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidExternCSignature,
                diagnostics::DiagnosticSeverity::Error,
                parameter.range,
                "FFI buffer parameter '" + parameter.name +
                    "' links unknown length parameter '" +
                    parameter.ffiLengthParameterName + "'");
        else
        {
            parameter.ffiLengthParameterOrdinal = static_cast<std::size_t>(
                std::distance(symbol.parameters.begin(), target));
            if (*parameter.ffiLengthParameterOrdinal == parameterOrdinal ||
                !ProjectTypeConversion::isIntegral(target->typeKey))
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidExternCSignature,
                    diagnostics::DiagnosticSeverity::Error,
                    parameter.range,
                    "FFI buffer length link for '" + parameter.name +
                        "' must name a distinct integral parameter");
        }
        if (!ProjectTypeConversion::isPointer(parameter.typeKey) ||
            ProjectTypeConversion::isFunctionPointer(parameter.typeKey))
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidExternCSignature,
                diagnostics::DiagnosticSeverity::Error,
                parameter.range,
                "length(...) requires a data-pointer parameter");
    }
    if (!parameter.ffiCallback) return;
    const NamedTypeDeclaration* callbackType =
        namedTypeForKey(parameter.typeKey, nameLookup);
    const bool externDelegate = callbackType != nullptr &&
        callbackType->kind == NamedTypeDeclarationKind::Delegate &&
        callbackType->syntax != nullptr &&
        callbackType->syntax->hasModifier(lexer::TokenKind::KwExtern);
    if (!ProjectTypeConversion::isFunctionPointer(parameter.typeKey) && !externDelegate)
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidExternCSignature,
            diagnostics::DiagnosticSeverity::Error,
            parameter.range,
            "callback(...) requires a function pointer or extern C delegate parameter");
    if (parameter.ffiCallbackMayEscape &&
        parameter.ownership != sema::OwnershipKind::Owned)
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidExternCSignature,
            diagnostics::DiagnosticSeverity::Error,
            parameter.range,
            "callback(escaping) requires an owned callback lifetime transfer");
}

void ProjectMemberModel::validateReferenceReturnLifetime(
    ClassMemberSymbol& symbol,
    const NamedTypeDeclaration& type,
    const ast::AstNode* memberNode,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!symbol.isReferenceReturn)
    {
        if (const auto cycle = lifetimeCycle(symbol.lifetimeOutlivesConstraints))
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                symbol.range,
                "lifetime constraints contain a cycle between '" + cycle->first +
                    "' and '" + cycle->second + "'");
        return;
    }
    const bool inferredLifetime = symbol.returnLifetimeName.empty();
    if (inferredLifetime) inferReferenceReturnLifetime(symbol, diagnostics);
    else if (symbol.returnLifetimeName == "this" && !symbol.hasImplicitThis)
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidReturn,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "lifetime 'this' is available only on instance ref returns");
    else if (symbol.returnLifetimeName != "static" &&
             !declaresLifetime(memberNode, symbol.returnLifetimeName) &&
             !declaresLifetime(type.syntax, symbol.returnLifetimeName))
        diagnostics.report(
            diagnostics::DiagnosticId::UnknownIdentifier,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "ref return of '" + symbol.name + "' names undeclared lifetime '" +
                symbol.returnLifetimeName + "'");
    if (symbol.returnLifetimeName != "this" &&
        symbol.returnLifetimeName != "static" &&
        !symbol.returnLifetimeName.empty())
    {
        const bool hasSource = std::ranges::any_of(
            symbol.parameters,
            [&](const MethodParameterSymbol& parameter)
            {
                return (parameter.passKind == ParameterPassKind::Ref ||
                        parameter.passKind == ParameterPassKind::In) &&
                    lifetimeOutlives(
                        symbol.lifetimeOutlivesConstraints,
                        parameter.lifetimeName,
                        symbol.returnLifetimeName);
            });
        if (!hasSource)
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidReturn,
                diagnostics::DiagnosticSeverity::Error,
                symbol.range,
                "ref return lifetime '" + symbol.returnLifetimeName +
                    "' has no ref/in parameter whose lifetime outlives it");
    }
    if (const auto cycle = lifetimeCycle(symbol.lifetimeOutlivesConstraints))
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "lifetime constraints contain a cycle between '" + cycle->first +
                "' and '" + cycle->second + "'");
}

void ProjectMemberModel::buildCallableSignature(
    ClassMemberSymbol& symbol,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    // C4: a by-reference parameter is part of the signature. `F(int)` and
    // `F(ref int)` are different members, and the signature key has to say so or the
    // two would collide as duplicates.
    symbol.signatureKey = symbol.name + '(';
    for (std::size_t parameterIndex = 0;
         parameterIndex < symbol.parameters.size(); ++parameterIndex)
    {
        if (parameterIndex != 0) symbol.signatureKey.push_back(',');
        symbol.signatureKey += ownershipSignaturePrefix(
            symbol.parameters[parameterIndex].ownership);
        // C4: `F(int* p)` and `F(ref int p)` lower to the same machine signature but
        // are different members of the language -- the second one demands the keyword
        // at every call site. Without this prefix the two would collide as duplicates.
        if (symbol.parameters[parameterIndex].passKind != ParameterPassKind::Value)
        {
            symbol.signatureKey += parameterPassKindKeyword(
                symbol.parameters[parameterIndex].passKind);
            symbol.signatureKey.push_back(' ');
        }
        symbol.signatureKey += symbol.parameters[parameterIndex].typeKey;
    }
    symbol.signatureKey.push_back(')');
    if (symbol.isVariadic)
        symbol.signatureKey.insert(symbol.signatureKey.size() - 1, ",...");
    // Unlike an ordinary method, a conversion operator is identified by its
    // source AND target types. Multiple conversions may therefore share
    // `op_Implicit(Source)` while producing distinct target types.
    if (symbol.isUserConversion)
        symbol.signatureKey += "->" + symbol.declaredTypeKey;
    if (!symbol.isUserOperator) return;
    const language::OperatorTraitContract* contract =
        language::operatorTraitContractForMethod(
            symbol.name, symbol.parameters.size());
    if (contract != nullptr) symbol.operatorTraitKey = std::string(contract->langItem);
    else
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidOperand,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "user-defined operator has no NativeSharp 3 lang-item contract");
}

void ProjectMemberModel::validateCAbiMember(
    const ClassMemberSymbol& symbol,
    const NamedTypeDeclaration& type,
    const ast::AstNode* memberNode,
    const ProjectNameLookup& nameLookup,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!symbol.isExported && !symbol.isExternC) return;
    // Interop Foundation: an IMPORTED C function (`extern C from "<dll>"`) shares the
    // C-ABI surface rules with exports -- same type validation below, same reserved-
    // name protection -- but is deliberately NOT an export: it has no body here, its
    // definition lives in the named DLL, and it must not demand the `export` half of
    // the Stage 47 pairing.
    if (!symbol.isExportedC && !symbol.isImportedC)
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "Stage 47 exported host methods require the exact paired modifiers 'export extern C'");
    if (symbol.kind != ClassMemberKind::Method ||
        symbol.accessibility != ClassMemberAccessibility::Public ||
        !symbol.isStatic || symbol.isAbstract || symbol.isCompilerGenerated ||
        symbol.isPropertyAccessor || symbol.isIndexerAccessor ||
        symbol.isUserOperator || symbol.isUserConversion)
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "Stage 47 export extern C requires an ordinary public static concrete method");
    if (!memberNode->genericParameters().empty() ||
        (type.syntax != nullptr && !type.syntax->genericParameters().empty()))
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "Stage 47 export extern C does not expose generic methods or members of generic types");
    if (isReservedStage47CAbiExportName(symbol.cAbiName))
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "Stage 47 C ABI export name '" + symbol.cAbiName +
                "' collides with a reserved NativeSharp/runtime/toolchain symbol");
    // C1.1 "function-pointer round trip": an ORDINARY delegate is a 16-byte heap cell
    // (code + receiver) and has no C shape, so returning one across the boundary stays
    // rejected. An EXTERN C delegate is a raw C function pointer end to end (see
    // lowerDelegateCreate: binding a static method to one yields the function's own
    // address), so it is exactly as returnable as `void*` -- which is what makes
    // `wglGetProcAddress`/`SDL_GL_GetProcAddress`-shaped declarations expressible with
    // their real signature instead of an untyped `nint` the caller must blind-cast.
    const NamedTypeDeclaration* exportedReturnType =
        namedTypeForKey(symbol.declaredTypeKey, nameLookup);
    if (exportedReturnType != nullptr &&
        exportedReturnType->kind == NamedTypeDeclarationKind::Delegate &&
        !(exportedReturnType->syntax != nullptr &&
          exportedReturnType->syntax->hasModifier(lexer::TokenKind::KwExtern)))
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidExternCSignature,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "Stage 47 C ABI does not return delegate values; return an extern C delegate "
            "(a raw function pointer) instead");
    if (!isStage47CAbiType(symbol.declaredTypeKey, nameLookup, true))
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidExternCSignature,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "Stage 47 C ABI cannot export return type '" + symbol.declaredTypeKey +
                "'; use scalars, enums, small plain structs, raw pointers or extern C delegates");
    for (const MethodParameterSymbol& parameter : symbol.parameters)
    {
        if (isStage47CAbiType(parameter.typeKey, nameLookup, false)) continue;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidExternCSignature,
            diagnostics::DiagnosticSeverity::Error,
            parameter.range,
            "Stage 47 C ABI cannot export parameter type '" + parameter.typeKey +
                "'; pass strings/arrays through explicit pointer+length boundaries; "
                "class/interface references are opaque handles");
    }
}

void ProjectMemberModel::validateUserOperator(
    const ClassMemberSymbol& symbol,
    const NamedTypeDeclaration& type,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    if (!symbol.isUserOperator) return;
    const std::string containingTypeKey = "type:" + type.identity.key();
    bool containsContainingType = false;
    for (const MethodParameterSymbol& parameter : symbol.parameters)
    {
        const std::string_view operandType = parameter.passKind == ParameterPassKind::Value
            ? std::string_view(parameter.typeKey)
            : std::string_view(parameter.pointeeTypeKey);
        containsContainingType = containsContainingType || operandType == containingTypeKey;
        if (parameter.passKind == ParameterPassKind::Out)
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidOperand,
                diagnostics::DiagnosticSeverity::Error,
                parameter.range,
                "operator operands may be passed by value, ref or in, but never out");
    }
    if (!containsContainingType && !(symbol.isAbstract && isTraitDeclaration(type)))
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "user-defined operator '" + symbol.name +
                "' must declare at least one operand of the containing type");
    const std::string& metadata = symbol.syntax->auxiliaryText();
    const bool comparison = metadata == "operator:EqualEqual" ||
        metadata == "operator:BangEqual" || metadata == "operator:Less" ||
        metadata == "operator:LessEqual" || metadata == "operator:Greater" ||
        metadata == "operator:GreaterEqual";
    if (comparison && symbol.declaredTypeKey != "builtin:bool")
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            symbol.range,
            "comparison/equality user-defined operators must return bool in Stage 38");
}
}
