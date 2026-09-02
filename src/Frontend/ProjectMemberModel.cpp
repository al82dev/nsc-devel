#include "nativesharp/Frontend/ProjectMemberModel.h"

#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <cctype>
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
        modifiers.front().kind == ast::TypeModifierSyntaxKind::DynamicArray;
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

std::vector<std::size_t> ProjectMemberModel::findMembers(
    std::size_t typeOrdinal,
    std::string_view name) const
{
    std::vector<std::size_t> result;
    if (typeOrdinal >= membersByType_.size())
    {
        return result;
    }

    for (const std::size_t memberOrdinal : membersByType_[typeOrdinal])
    {
        if (memberOrdinal < members_.size() && members_[memberOrdinal].name == name)
        {
            result.push_back(memberOrdinal);
        }
    }
    return result;
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
            if (directMember != nullptr && directMember->kind() == ast::AstNodeKind::PropertyDeclaration)
            {
                const auto ordinary = ordinaryDeclarations.find(directMember->text());
                const auto previousProperty = propertyDeclarations.find(directMember->text());
                if (ordinary != ordinaryDeclarations.end() || previousProperty != propertyDeclarations.end())
                {
                    const ast::AstNode* previous = ordinary != ordinaryDeclarations.end()
                        ? ordinary->second : previousProperty->second;
                    diagnostics.report(
                        diagnostics::DiagnosticId::DuplicateSymbol,
                        diagnostics::DiagnosticSeverity::Error,
                        directMember->range(),
                        "property name '" + directMember->text() + "' conflicts with an existing member in type '" +
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
                    propertyDeclarations.emplace(directMember->text(), directMember);
                }
            }
            else if (directMember != nullptr &&
                     (directMember->kind() == ast::AstNodeKind::FieldDeclaration ||
                      directMember->kind() == ast::AstNodeKind::EventDeclaration ||
                      directMember->kind() == ast::AstNodeKind::MethodDeclaration))
            {
                if (propertyDeclarations.contains(directMember->text()))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::DuplicateSymbol,
                        diagnostics::DiagnosticSeverity::Error,
                        directMember->range(),
                        "member name '" + directMember->text() + "' conflicts with an existing property in type '" +
                            type.identity.fullyQualifiedName() + "'");
                }
                ordinaryDeclarations.emplace(directMember->text(), directMember);
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
            symbol.isConst = symbol.kind == ClassMemberKind::Field &&
                memberNode->hasModifier(lexer::TokenKind::KwConst);
            symbol.isStatic = (symbol.kind == ClassMemberKind::Field || symbol.kind == ClassMemberKind::Event ||
                symbol.kind == ClassMemberKind::Method) &&
                (memberNode->hasModifier(lexer::TokenKind::KwStatic) || symbol.isConst);
            symbol.isReadonly = symbol.kind == ClassMemberKind::Field &&
                memberNode->hasModifier(lexer::TokenKind::KwReadonly);
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

            const std::string& metadata = memberNode->auxiliaryText();
            symbol.isCompilerGenerated = metadata.starts_with("property-backing:") ||
                metadata.starts_with("event-backing:");
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
                if (fixedArrayElementTypeKey(symbol.declaredTypeKey).has_value())
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        symbol.range,
                        "a fixed array cannot be returned by value; return a pointer to its element type");
                }
                for (const MethodParameterSymbol& parameter : symbol.parameters)
                {
                    if (!fixedArrayElementTypeKey(parameter.typeKey).has_value()) continue;
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        parameter.range,
                        "a fixed array cannot be passed by value; declare the parameter as a pointer to its element type");
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
                                (symbol.kind == ClassMemberKind::Method
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
            memberBySyntax_.emplace(memberNode, memberOrdinal);
            byName[members_[memberOrdinal].name].push_back(memberOrdinal);
        }
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
bool ProjectMemberModel::collectEnumMembers(
    std::size_t typeOrdinal,
    const NamedTypeDeclaration& type,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (type.syntax == nullptr)
    {
        return !diagnostics.hasErrors();
    }

    std::unordered_map<std::string, const ast::AstNode*> seen;
    std::int64_t nextValue = 0;
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

        std::int64_t value = nextValue;
        if (memberNode->first() != nullptr)
        {
            const ast::AstNode* initializer = memberNode->first();
            bool negative = false;
            if (initializer->kind() == ast::AstNodeKind::UnaryExpression &&
                initializer->tokenKind() == lexer::TokenKind::Minus && initializer->first() != nullptr)
            {
                negative = true;
                initializer = initializer->first();
            }
            if (initializer->kind() != ast::AstNodeKind::IntegerLiteralExpression)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidEnumInitializer,
                    diagnostics::DiagnosticSeverity::Error,
                    memberNode->range(),
                    "enum initializer for '" + memberNode->text() +
                        "' must be a plain integer literal in the Stage 51.x OOP pipeline "
                        "(general constant-expression folding for enum initializers is not supported yet)");
                continue;
            }
            const std::uint64_t magnitude = initializer->integerValue();
            const std::int64_t signedMagnitude = static_cast<std::int64_t>(magnitude);
            value = negative ? -signedMagnitude : signedMagnitude;
            if (value < std::numeric_limits<std::int32_t>::min() ||
                value > std::numeric_limits<std::int32_t>::max())
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidEnumInitializer,
                    diagnostics::DiagnosticSeverity::Error,
                    memberNode->range(),
                    "enum value for '" + memberNode->text() + "' must be representable as NativeSharp int");
                continue;
            }
        }
        nextValue = value + 1;

        ClassMemberSymbol symbol;
        symbol.kind = ClassMemberKind::Field;
        symbol.accessibility = ClassMemberAccessibility::Public;
        symbol.declaringTypeOrdinal = typeOrdinal;
        symbol.fileId = type.fileId;
        symbol.range = memberNode->range();
        symbol.syntax = memberNode;
        symbol.sourcePath = type.parts.empty() ? std::string{} : type.parts.front().sourcePath;
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
        symbol.enumConstantValue = value;

        const std::size_t memberOrdinal = members_.size();
        members_.push_back(std::move(symbol));
        membersByType_[typeOrdinal].push_back(memberOrdinal);
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
        std::vector<std::size_t> candidates = findMembers(typeOrdinal, node->text());
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
