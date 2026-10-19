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
const std::string* ProjectMemberModel::resolvedGenericTypeIdentity(
    const ast::TypeSyntax* syntax) const
{
    // NS304-T08-F06: an OPEN generic reference (`Box<T>` with a still-present type argument) carries
    // its recorded identity from the generic model; use it instead of failing name resolution.
    if (genericModel_ == nullptr || syntax->typeArguments().empty()) return nullptr;
    const std::string* identity = genericModel_->resolvedTypeIdentity(syntax);
    return identity != nullptr && !identity->empty() ? identity : nullptr;
}

std::string ProjectMemberModel::canonicalFunctionTypeKey(
    const ast::TypeSyntax* syntax,
    const ProjectNameLookup& nameLookup) const
{
    language::FunctionType function;
    function.carrier = syntax->functionCarrier() ==
            ast::FunctionTypeCarrierSyntaxKind::Pointer
        ? language::FunctionCarrier::Pointer
        : language::FunctionCarrier::Reference;
    switch (syntax->functionCallingConvention())
    {
        case ast::FunctionCallingConventionSyntaxKind::NativeSharp:
            function.callingConvention = language::CallingConvention::NativeSharp;
            break;
        case ast::FunctionCallingConventionSyntaxKind::C:
            function.callingConvention = language::CallingConvention::C;
            break;
        case ast::FunctionCallingConventionSyntaxKind::StdCall:
            function.callingConvention = language::CallingConvention::StdCall;
            break;
        case ast::FunctionCallingConventionSyntaxKind::FastCall:
            function.callingConvention = language::CallingConvention::FastCall;
            break;
        case ast::FunctionCallingConventionSyntaxKind::VectorCall:
            function.callingConvention = language::CallingConvention::VectorCall;
            break;
        case ast::FunctionCallingConventionSyntaxKind::Win64:
            function.callingConvention = language::CallingConvention::Win64;
            break;
    }
    function.parameters.reserve(syntax->typeArguments().size());
    for (const ast::TypeSyntax* parameter : syntax->typeArguments())
        function.parameters.push_back(canonicalTypeKey(parameter, nameLookup));
    function.result = canonicalTypeKey(syntax->functionReturnType(), nameLookup);
    function.errorType = syntax->functionErrorType() == nullptr
        ? std::string{}
        : canonicalTypeKey(syntax->functionErrorType(), nameLookup);
    function.lifetime = syntax->functionLifetimeName();
    function.variadic = syntax->functionIsVariadic();
    function.isUnsafe = syntax->functionIsUnsafe();
    function.isNoHeap = syntax->functionIsNoHeap();
    function.isAsync = syntax->functionIsAsync();
    function.nullable = syntax->functionIsNullable();
    return language::functionTypeIdentity(function);
}

std::string ProjectMemberModel::canonicalTupleTypeKey(
    const ast::TypeSyntax* syntax,
    const ProjectNameLookup& nameLookup) const
{
    std::vector<language::TupleElement> elements;
    elements.reserve(syntax->typeArguments().size());
    for (std::size_t index = 0; index < syntax->typeArguments().size(); ++index)
    {
        elements.push_back(language::TupleElement{
            canonicalTypeKey(syntax->typeArguments()[index], nameLookup),
            index < syntax->tupleElementNames().size()
                ? syntax->tupleElementNames()[index] : std::string{}});
    }
    return language::tupleIdentity(elements);
}

std::string ProjectMemberModel::canonicalPortableSimdTypeKey(
    const ast::TypeSyntax* syntax,
    const ProjectNameLookup& nameLookup) const
{
    std::string markerType;
    std::string markerValue;
    std::size_t laneCount = 0;
    const bool marker = language::parseConstGenericArgumentMarker(
        syntax->typeArguments()[1]->baseName(), markerType, markerValue);
    if (marker)
    {
        const auto parsed = std::from_chars(
            markerValue.data(), markerValue.data() + markerValue.size(), laneCount);
        if (parsed.ec == std::errc{} && parsed.ptr == markerValue.data() + markerValue.size())
        {
            std::string error;
            const auto type = language::portableSimdType(
                canonicalTypeKey(syntax->typeArguments()[0], nameLookup), laneCount,
                syntax->baseName() == "mask", error);
            if (type.has_value()) return language::portableSimdTypeIdentity(*type);
        }
    }
    return "invalid-simd:" + syntax->baseName();
}

std::string ProjectMemberModel::canonicalNamedTypeKey(
    const ast::TypeSyntax* syntax,
    const ProjectNameLookup& nameLookup) const
{
    if (syntax->baseName() == "Future" && syntax->typeArguments().size() == 1)
    {
        // NS304-T05-F09: `Future<T>` is the opaque, nameable future surface. It maps to the
        // internal `future:<T>` type key (the non-throwing future).
        return language::futureTypeKey(
            canonicalTypeKey(syntax->typeArguments()[0], nameLookup));
    }
    if (syntax->baseName() == "Iterator" && syntax->typeArguments().size() == 1)
    {
        // NS304-T06-F02: `Iterator<T>` is the opaque, nameable generator iterator surface. It maps
        // to the internal `iterator:<T>` type key; T is the item type of every yield.
        return language::iteratorTypeKey(
            canonicalTypeKey(syntax->typeArguments()[0], nameLookup));
    }
    if (syntax->baseName() == "Iterator" && syntax->typeArguments().size() == 2)
    {
        // NS304-T06-F07: `Iterator<T,E>` carries the generator's error type (iterator:<T>|<E>).
        return language::iteratorTypeKey(
            canonicalTypeKey(syntax->typeArguments()[0], nameLookup),
            canonicalTypeKey(syntax->typeArguments()[1], nameLookup));
    }
    if (syntax->baseName() == "AsyncIterator" && syntax->typeArguments().size() == 2)
    {
        // NS304-T07-F01: `AsyncIterator<T,E>` is the opaque, nameable async-stream iterator. It maps
        // to `async-iterator:<T>|<E>`; T is the item, E the error.
        return language::asyncIteratorTypeKey(
            canonicalTypeKey(syntax->typeArguments()[0], nameLookup),
            canonicalTypeKey(syntax->typeArguments()[1], nameLookup));
    }
    if (lexer::isBuiltinTypeKeyword(syntax->baseTokenKind()) ||
        (syntax->baseName() == "decimal" && syntax->typeArguments().empty()))
        return "builtin:" + syntax->baseName();
    if (const NamedTypeDeclaration* declaration = nameLookup.resolvedDeclaration(syntax);
        declaration != nullptr)
        return "type:" + declaration->identity.key();

    // NS304-T08-F06: a specialized clone's receiver (`Box__g1__I__...`) materialized late may
    // not carry a name-lookup binding; resolve it by its mangled terminal name.
    for (const NamedTypeDeclaration& declaration : nameLookup.namedTypes())
    {
        if (declaration.identity.key().ends_with(syntax->baseName()))
            return "type:" + declaration.identity.key();
    }
    return "unresolved:" + syntax->baseName();
}

void ProjectMemberModel::appendCanonicalTypeModifier(
    std::string& result,
    const ast::TypeModifierSyntax& modifier) const
{
    if (modifier.kind == ast::TypeModifierSyntaxKind::Pointer)
        result += "*";
    else if (modifier.kind == ast::TypeModifierSyntaxKind::DynamicArray)
        result += "[]";
    else if (modifier.kind == ast::TypeModifierSyntaxKind::RangeArray)
    {
        result.push_back('[');
        if (modifier.arrayRank >= 2) result.append(modifier.arrayRank - 1, ',');
        result.push_back(']');
    }
    else if (modifier.kind == ast::TypeModifierSyntaxKind::FlexibleArray)
        result += "[*]";
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
            result += "[" + std::to_string(*count) + "]";
        else
            result += "[" + syntaxFingerprint(modifier.arrayCount) + "]";
    }
}

void ProjectMemberModel::appendCanonicalTypeModifiers(
    std::string& result,
    const ast::TypeSyntax* syntax) const
{
    for (const ast::TypeModifierSyntax& modifier : syntax->modifiers())
        appendCanonicalTypeModifier(result, modifier);
}

void ProjectMemberModel::applyCanonicalReferenceQualifier(
    std::string& result,
    const ast::TypeSyntax* syntax)
{
    // NS304-T06-F05: `ref T` / `ref mut T` — the item-by-reference qualifier prefixes the key.
    if (syntax->referenceQualifier() == ast::ReferenceQualifierSyntaxKind::Ref)
        result = "ref:" + result;
    else if (syntax->referenceQualifier() == ast::ReferenceQualifierSyntaxKind::RefMut)
        result = "refmut:" + result;
}

std::string ProjectMemberModel::canonicalTypeKey(
    const ast::TypeSyntax* syntax,
    const ProjectNameLookup& nameLookup) const
{
    if (syntax == nullptr) return "<missing-type>";
    if (const std::string* identity = resolvedGenericTypeIdentity(syntax)) return *identity;
    std::string result;
    if (syntax->isFunction())
        result = canonicalFunctionTypeKey(syntax, nameLookup);
    else if (syntax->isTuple())
        result = canonicalTupleTypeKey(syntax, nameLookup);
    else if ((syntax->baseName() == "simd" || syntax->baseName() == "mask") &&
             syntax->typeArguments().size() == 2)
        result = canonicalPortableSimdTypeKey(syntax, nameLookup);
    else
        result = canonicalNamedTypeKey(syntax, nameLookup);
    appendCanonicalTypeModifiers(result, syntax);
    applyCanonicalReferenceQualifier(result, syntax);
    return result;
}

std::string ProjectMemberModel::canonicalGenericArgumentKey(
    std::string_view spelling,
    const ProjectNameLookup& nameLookup)
{
    if (spelling.starts_with("builtin:") || spelling.starts_with("type:"))
        return std::string(spelling);

    constexpr std::pair<std::string_view, std::string_view> builtins[] = {
        {"void", "builtin:void"}, {"bool", "builtin:bool"},
        {"sbyte", "builtin:sbyte"}, {"byte", "builtin:byte"},
        {"char", "builtin:char"}, {"short", "builtin:short"},
        {"ushort", "builtin:ushort"}, {"int", "builtin:int"},
        {"uint", "builtin:uint"}, {"long", "builtin:long"},
        {"ulong", "builtin:ulong"}, {"nint", "builtin:nint"},
        {"nuint", "builtin:nuint"}, {"half", "builtin:half"},
        {"i128", "builtin:i128"}, {"u128", "builtin:u128"},
        {"float", "builtin:float"}, {"double", "builtin:double"},
        {"string", "builtin:string"}};
    for (const auto& [name, key] : builtins)
        if (spelling == name) return std::string(key);

    for (const std::string_view suffix : {std::string_view("[]"), std::string_view("*")})
    {
        if (!spelling.ends_with(suffix)) continue;
        const std::string base = canonicalGenericArgumentKey(
            spelling.substr(0, spelling.size() - suffix.size()), nameLookup);
        return base.starts_with("unresolved:") ? base : base + std::string(suffix);
    }

    const NamedTypeDeclaration* shortMatch = nullptr;
    bool ambiguousShortMatch = false;
    for (const NamedTypeDeclaration& type : nameLookup.namedTypes())
    {
        if (type.identity.key() == spelling || type.identity.fullyQualifiedName() == spelling)
            return "type:" + type.identity.key();
        if (type.identity.name() == spelling)
        {
            if (shortMatch != nullptr) ambiguousShortMatch = true;
            else shortMatch = &type;
        }
    }
    return shortMatch != nullptr && !ambiguousShortMatch
        ? "type:" + shortMatch->identity.key()
        : "unresolved:" + std::string(spelling);
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

}
