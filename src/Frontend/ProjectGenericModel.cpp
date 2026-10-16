#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectOpaqueResultModel.h"

#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
ProjectGenericModel::ProjectGenericModel(
    std::size_t maximumInstantiationDepth,
    std::size_t maximumInstantiationCount) noexcept
    : instantiationBudget_(maximumInstantiationDepth, maximumInstantiationCount)
{
}

namespace
{
constexpr std::string_view kGenericRawNewAuxiliary = "stage52-generic-raw-new";

// NSC305-P01. Times one specialization materialization and, on completion, records the
// `generics.specialization-waves` sub-phase and attributes its exclusive wall time to the
// template that produced it. RAII keeps every early-return path (budget, pack and associated
// rollback) accounted for without changing control flow.
class SpecializationWaveTimer final
{
public:
    SpecializationWaveTimer(GenericTelemetry& telemetry, std::string_view canonicalKey)
        : telemetry_(&telemetry), canonicalKey_(canonicalKey),
          started_(telemetry.enabled ? std::chrono::steady_clock::now()
                                     : std::chrono::steady_clock::time_point{})
    {
    }
    ~SpecializationWaveTimer()
    {
        if (!telemetry_->enabled) return;
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - started_);
        telemetry_->recordSubPhase("generics.specialization-waves", elapsed);
        telemetry_->noteMaterialization(canonicalKey_, elapsed);
    }
    SpecializationWaveTimer(const SpecializationWaveTimer&) = delete;
    SpecializationWaveTimer& operator=(const SpecializationWaveTimer&) = delete;

private:
    GenericTelemetry* telemetry_;
    std::string_view canonicalKey_;
    std::chrono::steady_clock::time_point started_;
};

bool isGenericTypeTemplateNode(const ast::AstNode* node) noexcept
{
    if (node == nullptr || node->genericParameters().empty()) return false;
    return node->kind() == ast::AstNodeKind::ClassDeclaration ||
        node->kind() == ast::AstNodeKind::StructDeclaration ||
        node->kind() == ast::AstNodeKind::InterfaceDeclaration ||
        node->kind() == ast::AstNodeKind::DelegateDeclaration;
}

bool isNamedTypeDeclaration(ast::AstNodeKind kind) noexcept
{
    return kind == ast::AstNodeKind::ClassDeclaration ||
        kind == ast::AstNodeKind::InterfaceDeclaration ||
        kind == ast::AstNodeKind::StructDeclaration ||
        kind == ast::AstNodeKind::EnumDeclaration ||
        kind == ast::AstNodeKind::DelegateDeclaration;
}

bool isStructuralGenericSurface(const ast::TypeSyntax* syntax) noexcept
{
    if (syntax == nullptr) return false;
    const std::size_t arity = syntax->typeArguments().size();
    if (syntax->isTuple()) return true;
    if (syntax->baseName() == "simd" || syntax->baseName() == "mask") return true;
    if (syntax->baseName() == "Future") return arity == 1U;
    if (syntax->baseName() == "Iterator") return arity == 1U || arity == 2U;
    return syntax->baseName() == "AsyncIterator" && arity == 2U;
}

GenericTemplateKind genericTypeTemplateKind(const ast::AstNode* node) noexcept
{
    if (node->kind() == ast::AstNodeKind::ClassDeclaration) return GenericTemplateKind::Class;
    if (node->kind() == ast::AstNodeKind::InterfaceDeclaration) return GenericTemplateKind::Interface;
    if (node->kind() == ast::AstNodeKind::DelegateDeclaration) return GenericTemplateKind::Delegate;
    return GenericTemplateKind::Struct;
}

// ID 132. Cuantos parametros DE VALOR declara un metodo. La identidad de una plantilla generica
// era `nombre + numero de parametros de TIPO` y nada mas, asi que dos metodos genericos con el
// mismo nombre y la misma aridad generica chocaban aunque tomaran cosas distintas:
//
//     duplicate generic template identity 'NativeSharp.Core.Spans.Of``1'
//
// Eso obligo a `Spans.OfRange` en vez de `Spans.Of` y a `Algorithm.SortWith` en vez de una
// sobrecarga de `Sort` -- dos nombres peores, escritos a proposito y anotados en el codigo.
std::size_t valueParameterCount(const ast::AstNode* node) noexcept
{
    if (node == nullptr) return 0;
    std::size_t count = 0;
    for (const ast::AstNode* child : node->children())
    {
        if (child != nullptr && child->kind() == ast::AstNodeKind::ParameterDeclaration) ++count;
    }
    return count;
}

bool isGenericMethodTemplateNode(const ast::AstNode* node) noexcept
{
    return node != nullptr &&
        (node->kind() == ast::AstNodeKind::MethodDeclaration ||
         node->kind() == ast::AstNodeKind::FunctionDeclaration) &&
        !node->genericParameters().empty();
}

GenericTemplateAccessibility genericTemplateAccessibility(
    const ast::AstNode* node,
    bool isMethod) noexcept
{
    if (node == nullptr)
        return isMethod ? GenericTemplateAccessibility::Private
                        : GenericTemplateAccessibility::Internal;
    switch (node->tokenKind())
    {
        case lexer::TokenKind::KwPublic:
            return GenericTemplateAccessibility::Public;
        case lexer::TokenKind::KwProtected:
            return GenericTemplateAccessibility::Protected;
        case lexer::TokenKind::KwPrivate:
            return GenericTemplateAccessibility::Private;
        case lexer::TokenKind::KwInternal:
        case lexer::TokenKind::Invalid:
        default:
            return isMethod ? GenericTemplateAccessibility::Private
                            : GenericTemplateAccessibility::Internal;
    }
}

std::string qualifiedName(std::string_view ns, std::string_view name)
{
    if (ns.empty()) return std::string(name);
    return std::string(ns) + "." + std::string(name);
}

// NSC-LIMIT-009. La cadena de tipos contenedores extendida con uno mas, unida con `+` igual que
// `TypeIdentity::fullyQualifiedName`.
//
// Este modelo llevaba UN nombre de dueno --el del contenedor mas interno-- y lo REEMPLAZABA al
// bajar, de modo que `A.Inner` y `B.Inner` eran los dos «Inner». Con eso, dos plantillas sin
// ninguna relacion compartian clave canonica, se agrupaban como sobrecargas la una de la otra y
// podian compartir especializacion. La cadena entera es lo unico que las separa.
std::string appendOwner(std::string_view chain, std::string_view name)
{
    if (chain.empty()) return std::string(name);
    return std::string(chain) + "+" + std::string(name);
}

// El receptor de `N1.C.F<int>(x)` TAL COMO SE ESCRIBIO: `N1.C`. Con puntos, sin interpretar.
//
// LA VERSION ANTERIOR LO CONVERTIA A `N1+C`, y eso era un error de principio, no de detalle: esta
// fase corre ANTES de la resolucion de nombres, de modo que no puede saber que en `N1.C` el primero
// es un namespace y el segundo un tipo, ni que en `A.Inner` los dos son tipos. Poner `+` es afirmar
// una de las dos lecturas sin datos para elegir.
//
// Medido con la version anterior: `N1.C.F<int>(1)` producia `N1+C`, que no coincide con NINGUN
// registro --el de N1 tiene owner `C`-- de modo que el filtro de dueno descartaba las dos
// plantillas y solo el fallback rescataba la llamada. Con dos `F<T>` visibles de la misma aridad,
// ese fallback no es ambiguo a veces: lo es siempre, y dos llamadas completamente calificadas no
// compilaban.
//
// Devuelve vacio si el receptor no es una cadena de identificadores --una expresion, un indice, una
// llamada--; entonces se conserva el comportamiento anterior.
std::string receiverSourceSpelling(const ast::AstNode* receiver)
{
    if (receiver == nullptr) return {};
    if (receiver->kind() == ast::AstNodeKind::IdentifierExpression) return receiver->text();
    if (receiver->kind() != ast::AstNodeKind::MemberExpression) return {};
    const std::string prefix = receiverSourceSpelling(receiver->first());
    if (prefix.empty()) return {};
    return prefix + "." + receiver->text();
}

// La escritura fuente COMPLETA de una plantilla: `N1.C`, `a3.A.Inner`. El registro guarda el
// namespace y la cadena de contenedores por separado, y la cadena usa `+` porque es la identidad
// canonica; aqui se vuelve a puntos, que es lo que el programador escribe.
std::string templateSourceSpelling(std::string_view namespaceName, std::string_view ownerChain)
{
    std::string owner(ownerChain);
    for (char& character : owner)
    {
        if (character == '+') character = '.';
    }
    return qualifiedName(namespaceName, owner);
}

// LOS SEGMENTOS DE UN NOMBRE PUNTEADO. `A.B` -> {"A","B"}; el vacio no da segmentos.
std::vector<std::string> splitDotted(std::string_view text)
{
    std::vector<std::string> segments;
    std::size_t begin = 0;
    while (begin <= text.size())
    {
        const std::size_t dot = text.find('.', begin);
        const std::size_t stop = dot == std::string_view::npos ? text.size() : dot;
        if (stop > begin) segments.emplace_back(text.substr(begin, stop - begin));
        if (dot == std::string_view::npos) break;
        begin = dot + 1;
    }
    return segments;
}

// LA ESCRITURA COMPLETA DE CADA TIPO CONTENEDOR DEL SITIO, de fuera adentro. El modelo guarda la
// cadena de duenos con `+` --es la identidad canonica-- y aqui hace falta con puntos y ACUMULADA:
// dentro de `A.Outer+Medio` los contenedores son `A.Outer` y `A.Outer.Medio`, en ese orden, que es
// lo que el escalon de contenedores necesita para probar de dentro afuera.
std::vector<std::string> enclosingTypeSpellings(
    std::string_view namespaceName, std::string_view ownerChain)
{
    std::vector<std::string> spellings;
    std::string current(namespaceName);
    std::size_t begin = 0;
    while (begin <= ownerChain.size())
    {
        const std::size_t plus = ownerChain.find('+', begin);
        const std::size_t stop = plus == std::string_view::npos ? ownerChain.size() : plus;
        if (stop > begin)
        {
            if (!current.empty()) current.push_back('.');
            current.append(ownerChain.substr(begin, stop - begin));
            spellings.push_back(current);
        }
        if (plus == std::string_view::npos) break;
        begin = plus + 1;
    }
    return spellings;
}

std::string stableHashSuffix(std::string_view text)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (const char raw : text)
    {
        const unsigned char ch = static_cast<unsigned char>(raw);
        hash ^= static_cast<std::uint64_t>(ch);
        hash *= 1099511628211ULL;
    }
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << hash;
    return stream.str();
}

class EnumConstResolver final : public ProjectConstantMemberResolver
{
public:
    EnumConstResolver(std::string enumName, std::string underlyingType)
        : enumName_(std::move(enumName)), underlyingType_(std::move(underlyingType))
    {
    }

    void add(std::string name, std::uint64_t bits)
    {
        const std::size_t ordinal = values_.size();
        names_.emplace(std::move(name), ordinal);
        values_.push_back(bits);
    }

    std::optional<std::size_t> resolveConstantMember(
        const ast::AstNode* syntax,
        std::optional<std::size_t>) const override
    {
        if (syntax == nullptr) return std::nullopt;
        if (syntax->kind() == ast::AstNodeKind::MemberExpression)
        {
            if (syntax->first() == nullptr ||
                syntax->first()->kind() != ast::AstNodeKind::IdentifierExpression ||
                syntax->first()->text() != enumName_)
                return std::nullopt;
        }
        else if (syntax->kind() != ast::AstNodeKind::IdentifierExpression)
        {
            return std::nullopt;
        }
        const auto found = names_.find(syntax->text());
        return found == names_.end()
            ? std::nullopt : std::optional<std::size_t>(found->second);
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
        if (memberOrdinal >= values_.size()) return false;
        isEnumConstant = true;
        enumConstantValue = static_cast<std::int64_t>(values_[memberOrdinal]);
        enumUnderlyingTypeKey = underlyingType_;
        initializer = nullptr;
        declaredTypeKey.clear();
        declaringTypeOrdinal.reset();
        return true;
    }

    std::string resolveTypeKey(const ast::TypeSyntax* syntax) const override
    {
        if (syntax == nullptr || !lexer::isBuiltinTypeKeyword(syntax->baseTokenKind())) return {};
        return "builtin:" + syntax->baseName();
    }

private:
    std::string enumName_;
    std::string underlyingType_;
    std::unordered_map<std::string, std::size_t> names_;
    std::vector<std::uint64_t> values_;
};

bool decodeConstGenericArgument(
    const ast::TypeSyntax* syntax,
    std::string& typeKey,
    std::string& value) noexcept
{
    return syntax != nullptr && syntax->typeArguments().empty() && syntax->modifiers().empty() &&
        language::parseConstGenericArgumentMarker(syntax->baseName(), typeKey, value);
}

ast::AstNode* constArgumentExpression(
    ast::AstArena& arena,
    std::string_view typeKey,
    std::string_view value,
    SourceRange range)
{
    if (typeKey.starts_with("tuple<"))
    {
        ast::AstNode* tuple = arena.make<ast::AstNode>(
            ast::AstNodeKind::TupleExpression, range);
        std::size_t cursor = 0;
        while (cursor < value.size())
        {
            const std::size_t colon = value.find(':', cursor);
            if (colon == std::string_view::npos || colon == cursor) return nullptr;
            std::size_t length = 0;
            const auto lengthResult = std::from_chars(
                value.data() + cursor, value.data() + colon, length);
            if (lengthResult.ec != std::errc{} || lengthResult.ptr != value.data() + colon)
                return nullptr;
            const std::size_t markerBegin = colon + 1U;
            if (length > value.size() - markerBegin) return nullptr;
            const std::size_t markerEnd = markerBegin + length;
            if (markerEnd >= value.size() || value[markerEnd] != ';') return nullptr;
            std::string elementType;
            std::string elementValue;
            if (!language::parseConstGenericArgumentMarker(
                    value.substr(markerBegin, length), elementType, elementValue))
                return nullptr;
            ast::AstNode* element = constArgumentExpression(
                arena, elementType, elementValue, range);
            if (element == nullptr) return nullptr;
            tuple->addChild(element);
            cursor = markerEnd + 1U;
        }
        return tuple;
    }

    const bool boolean = typeKey == "builtin:bool";
    if (!boolean && !value.empty() && value.front() == '-')
    {
        std::uint64_t magnitude = 0;
        const auto result = std::from_chars(
            value.data() + 1, value.data() + value.size(), magnitude);
        if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) return nullptr;
        ast::AstNode* operand = arena.make<ast::AstNode>(
            ast::AstNodeKind::IntegerLiteralExpression, range);
        operand->setText(std::string(value.substr(1)));
        operand->setTokenKind(lexer::TokenKind::IntegerLiteral);
        operand->setIntegerValue(magnitude);
        ast::AstNode* negation = arena.make<ast::AstNode>(
            ast::AstNodeKind::UnaryExpression, range);
        negation->setText(std::string(value));
        negation->setTokenKind(lexer::TokenKind::Minus);
        negation->setFirst(operand);
        return negation;
    }

    ast::AstNode* literal = arena.make<ast::AstNode>(
        boolean ? ast::AstNodeKind::BooleanLiteralExpression
                : ast::AstNodeKind::IntegerLiteralExpression,
        range);
    literal->setText(std::string(value));
    literal->setTokenKind(boolean
        ? (value == "true" ? lexer::TokenKind::KwTrue : lexer::TokenKind::KwFalse)
        : lexer::TokenKind::IntegerLiteral);
    if (!boolean)
    {
        std::uint64_t integer = 0;
        const auto result = std::from_chars(
            value.data(), value.data() + value.size(), integer);
        if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) return nullptr;
        literal->setIntegerValue(integer);
    }
    return literal;
}

bool normalizeConstExpressionArgument(
    ast::TypeSyntax* syntax,
    std::string& typeKey,
    std::string& value)
{
    if (syntax == nullptr || syntax->constExpression() == nullptr) return false;
    const ast::AstNode* expression = syntax->constExpression();
    if (expression->kind() == ast::AstNodeKind::TupleExpression)
    {
        std::string tupleType = "tuple<";
        std::string tupleValue;
        for (std::size_t index = 0; index < expression->children().size(); ++index)
        {
            ast::TypeSyntax elementCarrier(
                expression->children()[index]->range(), lexer::TokenKind::Invalid,
                "$nsc_const_expression$");
            elementCarrier.setConstExpression(expression->children()[index]);
            std::string elementType;
            std::string elementValue;
            if (!normalizeConstExpressionArgument(
                    &elementCarrier, elementType, elementValue)) return false;
            if (index != 0) tupleType += ',';
            tupleType += elementType;
            const std::string marker =
                language::constGenericArgumentMarker(elementType, elementValue);
            tupleValue += std::to_string(marker.size()) + ":" + marker + ";";
        }
        tupleType += '>';
        typeKey = std::move(tupleType);
        value = std::move(tupleValue);
        syntax->setBaseName(language::constGenericArgumentMarker(typeKey, value));
        syntax->setConstExpression(nullptr);
        return true;
    }
    if (expression->kind() == ast::AstNodeKind::BooleanLiteralExpression)
    {
        typeKey = "builtin:bool";
        value = expression->tokenKind() == lexer::TokenKind::KwTrue ? "true" : "false";
    }
    else
    {
        const ProjectIntegerEvaluator evaluator(nullptr);
        const std::optional<ProjectIntegerValue> evaluated = evaluator.evaluate(expression);
        if (!evaluated.has_value()) return false;
        typeKey = evaluated->typeKey;
        value = std::to_string(evaluated->bits);
    }
    syntax->setBaseName(language::constGenericArgumentMarker(typeKey, value));
    syntax->setConstExpression(nullptr);
    return true;
}

std::string constGenericParameterTypeKey(const ast::GenericParameterSyntax& parameter)
{
    if (parameter.constantType == nullptr) return {};
    return lexer::isBuiltinTypeKeyword(parameter.constantType->baseTokenKind())
        ? "builtin:" + parameter.constantType->baseName()
        : std::string{};
}

std::optional<std::size_t> trailingPackIndex(const GenericTemplateRecord& record) noexcept
{
    if (record.parameterSyntaxes.empty()) return std::nullopt;
    const std::size_t index = record.parameterSyntaxes.size() - 1U;
    return record.parameterSyntaxes[index].kind == ast::GenericParameterSyntaxKind::TypePack ||
        record.parameterSyntaxes[index].kind == ast::GenericParameterSyntaxKind::ConstantPack
        ? std::optional<std::size_t>(index) : std::nullopt;
}

std::string genericExpressionPattern(const ast::AstNode* node)
{
    if (node == nullptr) return "-";
    std::string result = std::to_string(static_cast<unsigned>(node->kind())) + ":" +
        std::to_string(static_cast<unsigned>(node->tokenKind())) + ":" + node->text() + ":" +
        std::to_string(node->integerValue());
    result += "[" + genericExpressionPattern(node->first()) + "]";
    result += "[" + genericExpressionPattern(node->second()) + "]";
    result += "[" + genericExpressionPattern(node->third()) + "]";
    result.push_back('{');
    for (const ast::AstNode* child : node->children())
    {
        result += genericExpressionPattern(child);
        result.push_back(';');
    }
    result.push_back('}');
    return result;
}

std::string genericTypePattern(
    const ast::TypeSyntax* syntax,
    std::span<const std::string> parameters)
{
    if (syntax == nullptr) return "<missing>";
    std::string result;
    if (syntax->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None)
    {
        result += std::string(ast::ownershipQualifierSyntaxKindName(syntax->ownershipQualifier()));
        result.push_back(' ');
    }
    if (syntax->isDynamicTrait()) result += "dyn ";
    if (syntax->isTuple())
    {
        result.push_back('(');
        for (std::size_t index = 0; index < syntax->typeArguments().size(); ++index)
        {
            if (index != 0) result.push_back(',');
            result += genericTypePattern(syntax->typeArguments()[index], parameters);
        }
        result.push_back(')');
    }
    else
    {
        const auto parameter = std::ranges::find(parameters, syntax->baseName());
        if (parameter != parameters.end() && syntax->typeArguments().empty())
        {
            result += "$" + std::to_string(
                static_cast<std::size_t>(parameter - parameters.begin()));
        }
        else
        {
            result += lexer::isBuiltinTypeKeyword(syntax->baseTokenKind())
                ? "builtin:" + syntax->baseName() : syntax->baseName();
            if (!syntax->typeArguments().empty())
            {
                result.push_back('<');
                for (std::size_t index = 0; index < syntax->typeArguments().size(); ++index)
                {
                    if (index != 0) result.push_back(',');
                    result += genericTypePattern(syntax->typeArguments()[index], parameters);
                }
                result.push_back('>');
            }
        }
    }
    if (syntax->isPointeeConst()) result += " const-pointee";
    if (syntax->isPackExpansion()) result += "...";
    for (const ast::TypeModifierSyntax& modifier : syntax->modifiers())
    {
        switch (modifier.kind)
        {
            case ast::TypeModifierSyntaxKind::Pointer: result.push_back('*'); break;
            case ast::TypeModifierSyntaxKind::DynamicArray: result += "[]"; break;
            case ast::TypeModifierSyntaxKind::RangeArray:
                result.push_back('[');
                if (modifier.arrayRank > 1) result.append(modifier.arrayRank - 1, ',');
                result.push_back(']');
                break;
            case ast::TypeModifierSyntaxKind::FlexibleArray: result += "[*]"; break;
            case ast::TypeModifierSyntaxKind::Array:
                result += "[" + genericExpressionPattern(modifier.arrayCount) + "]";
                break;
        }
    }
    return result;
}

std::string methodSpecializationFamilyKey(
    const GenericTemplateRecord& record,
    const ast::AstNode* syntax)
{
    std::string key = record.canonicalKey;
    key += syntax != nullptr && syntax->hasModifier(lexer::TokenKind::KwStatic)
        ? "|static(" : "|instance(";
    if (syntax != nullptr)
    {
        bool first = true;
        for (const ast::AstNode* child : syntax->children())
        {
            if (child == nullptr || child->kind() != ast::AstNodeKind::ParameterDeclaration)
                continue;
            if (!first) key.push_back(',');
            first = false;
            key += child->auxiliaryText();
            key.push_back(':');
            key += genericTypePattern(child->typeSyntax(), record.parameters);
        }
        if (syntax->isVariadic()) key += ",...";
    }
    key.push_back(')');
    return key;
}

bool genericArityAccepts(const GenericTemplateRecord& record, std::size_t arity) noexcept
{
    if (const auto pack = trailingPackIndex(record); pack.has_value())
        return arity >= *pack;
    return arity == record.parameters.size();
}

std::string genericAritySpelling(const GenericTemplateRecord& record)
{
    if (const auto pack = trailingPackIndex(record); pack.has_value())
        return std::to_string(*pack) + "+pack";
    return std::to_string(record.parameters.size());
}
}

const char* genericTemplateKindName(GenericTemplateKind kind) noexcept
{
    switch (kind)
    {
        case GenericTemplateKind::Class: return "class";
        case GenericTemplateKind::Struct: return "struct";
        case GenericTemplateKind::Delegate: return "delegate";
        case GenericTemplateKind::Method: return "method";
        case GenericTemplateKind::Interface: return "interface";
    }
    return "generic";
}

bool ProjectGenericModel::build(
    const compilation::ProjectGraph& graph,
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    std::span<const PrecompiledGenericSpecialization> precompiledSpecializations,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (built_) return !diagnostics.hasErrors();
    {
        const GenericTelemetry::Scope phase(telemetry_, "generics.visibility");
        buildVisibility(graph);
    }

    // Item 276. Generic aliases are transparent source spellings, so erase them before collecting
    // templates or dependency edges.  Doing it later would let `Alias<int>` acquire a bogus
    // specialization identity and would leave aliases inside template bodies until after cloning.
    {
        const GenericTelemetry::Scope phase(telemetry_, "generics.alias-expansion");
        for (const std::unique_ptr<ProjectSyntaxUnit>& unitPtr : units)
        {
            if (unitPtr == nullptr || unitPtr->syntaxTree() == nullptr) continue;
            expandAliasesInNode(
                *unitPtr, unitPtr->syntaxTree(), "", GenericLookupScope{}, diagnostics);
        }
    }
    if (diagnostics.hasErrors())
    {
        built_ = true;
        return false;
    }

    {
        const GenericTelemetry::Scope phase(telemetry_, "generics.trait-merge");
        if (!mergeTraitImplementations(units, diagnostics))
        {
            built_ = true;
            return false;
        }
    }
    {
        const GenericTelemetry::Scope phase(telemetry_, "generics.opaque-results");
        if (!ProjectOpaqueResultModel{}.build(units, diagnostics))
        {
            built_ = true;
            return false;
        }
    }
    ordinaryCallableReturnTypes_.clear();
    {
        const GenericTelemetry::Scope phase(telemetry_, "generics.ordinary-return-index");
    // NSC305-G06. Iterative walk (was a self-recursive lambda over the AST).
    std::vector<ast::AstNode*> ordinaryReturnStack;
    for (const std::unique_ptr<ProjectSyntaxUnit>& unit : units)
        if (unit != nullptr && unit->syntaxTree() != nullptr)
            ordinaryReturnStack.push_back(unit->syntaxTree());
    while (!ordinaryReturnStack.empty())
    {
        ast::AstNode* node = ordinaryReturnStack.back();
        ordinaryReturnStack.pop_back();
        if (node == nullptr) continue;
        if ((node->kind() == ast::AstNodeKind::MethodDeclaration ||
             node->kind() == ast::AstNodeKind::FunctionDeclaration) &&
            node->genericParameters().empty() && node->typeSyntax() != nullptr)
        {
            const auto [entry, inserted] = ordinaryCallableReturnTypes_.emplace(
                node->text(), node->typeSyntax());
            if (!inserted && entry->second != nullptr &&
                typeSpelling(entry->second) != typeSpelling(node->typeSyntax()))
                entry->second = nullptr;
        }
        const std::vector<ast::AstNode*> children(node->children().begin(), node->children().end());
        for (auto iterator = children.rbegin(); iterator != children.rend(); ++iterator)
            ordinaryReturnStack.push_back(*iterator);
        if (node->third() != nullptr) ordinaryReturnStack.push_back(node->third());
        if (node->second() != nullptr) ordinaryReturnStack.push_back(node->second());
        if (node->first() != nullptr) ordinaryReturnStack.push_back(node->first());
    }
    }
    {
        const GenericTelemetry::Scope phase(telemetry_, "generics.associated-items");
        if (!collectAssociatedItems(units, diagnostics))
        {
            built_ = true;
            return false;
        }
    }

    {
        const GenericTelemetry::Scope phase(telemetry_, "generics.template-collection");
        for (const std::unique_ptr<ProjectSyntaxUnit>& unitPtr : units)
        {
            if (unitPtr == nullptr || unitPtr->syntaxTree() == nullptr) continue;
            ProjectSyntaxUnit& unit = *unitPtr;
            collectTemplates(unit, unit.syntaxTree(), nullptr, "", "", false, diagnostics);
        }
    }

    // ID 132. LA DESAMBIGUACION SE APLICA SOLO DONDE HAY COLISION, y esto no es una optimizacion:
    // es lo que evita cambiar una cadena que YA ES PUBLICA.
    //
    // La primera version de este arreglo metia la cuenta de parametros en la clave SIEMPRE, y esa
    // clave sale del compilador por tres sitios: el volcado de HIR, el de MIR y los METADATOS DE
    // BIBLIOTECA que lee un host. Cuatro pruebas se pusieron rojas y tenian razon: cambiar el
    // formato de una identidad publica por un problema interno rompe a quien la lea.
    //
    // Asi que la clave de un metodo generico se queda EXACTAMENTE como estaba mientras sea unica.
    // Solo cuando dos plantillas comparten `nombre``aridad` se les anade `#<parametros de valor>`
    // a las dos, que es el unico caso donde antes habia un error y por tanto el unico donde no
    // puede haber nada escrito que dependa del formato anterior.
    {
        const GenericTelemetry::Scope phase(telemetry_, "generics.template-index");
        std::unordered_map<std::string, std::size_t> baseCounts;
        for (const TemplateState& state : templateStates_)
        {
            if (state.record.kind != GenericTemplateKind::Method) continue;
            ++baseCounts[state.record.canonicalKey];
        }
        // EL DESEMPATE ES LA POSICION DE DECLARACION, no la cuenta de parametros.
        //
        // La cuenta separaba `F<T>(T[])` de `F<T>(T[], int, int)`, pero NO separa `F<T>(T)` de
        // `F<T>(T[])` -- las dos toman uno. Y si dos plantillas comparten clave, comparten la
        // clave de especializacion, asi que la segunda daria en la cache de la primera y una de
        // las dos sobrecargas DESAPARECERIA en silencio. Ese es exactamente el fallo que este
        // proyecto persigue: no un rojo, sino un verde que mide otra cosa.
        //
        // El indice de declaracion es unico por construccion y determinista (orden de fuente).
        std::size_t index = 0;
        for (TemplateState& state : templateStates_)
        {
            const std::size_t position = index++;
            if (state.record.kind != GenericTemplateKind::Method) continue;
            const auto found = baseCounts.find(state.record.canonicalKey);
            if (found == baseCounts.end() || found->second < 2) continue;
            state.record.canonicalKey += "#" + std::to_string(position);
        }

        // A closed member from a controlled family is an implementation artifact, not an
        // additional public overload. Mark every participant (including the unmodified fallback)
        // so NSMETA can publish the open templates without trying to merge equal closed language
        // signatures.
        std::unordered_set<std::string> controlledFamilies;
        for (const TemplateState& state : templateStates_)
            if (state.record.kind == GenericTemplateKind::Method &&
                state.record.isControlledSpecialization)
                controlledFamilies.insert(state.record.specializationFamilyKey);
        for (TemplateState& state : templateStates_)
            state.record.isControlledSpecializationFamily =
                state.record.kind == GenericTemplateKind::Method &&
                controlledFamilies.contains(state.record.specializationFamilyKey);
    }

    // NSC305-G01. Build the O(1) syntax and canonical-key indexes now that template collection and
    // canonical-key disambiguation are final. `templateStates_` is stable from here on, so the
    // stored element pointers remain valid. `emplace` keeps the first state for a duplicated key,
    // matching the previous linear scan's "first match" semantics (duplicate type keys are
    // diagnosed just below, so their winner is irrelevant to a successful build).
    for (TemplateState& state : templateStates_)
    {
        stateBySyntax_.emplace(state.syntax, &state);
        stateByCanonicalKey_.emplace(state.record.canonicalKey, &state);
        if (state.record.kind != GenericTemplateKind::Method)
        {
            typesByModuleQualified_[
                state.record.moduleName + "::" +
                qualifiedName(state.record.namespaceName, state.record.qualifiedTypeName)]
                .push_back(&state);
            typesByVisibleName_[state.record.name].push_back(&state);
        }
        else
        {
            methodsByName_[state.record.name].push_back(&state);
        }
    }

    // Record the declaration-level dependency graph before any template is cloned. A producer
    // can then publish exactly the fixed-point closure of its public roots, and the NSMETA
    // verifier can prove that a same-module edge was not silently dropped.
    {
        const GenericTelemetry::Scope phase(telemetry_, "generics.dependency-discovery");
        for (const std::unique_ptr<ProjectSyntaxUnit>& unitPtr : units)
        {
            if (unitPtr == nullptr || unitPtr->syntaxTree() == nullptr) continue;
            discoverTemplateDependencies(
                *unitPtr, unitPtr->syntaxTree(), "", "", GenericLookupScope{});
        }
        for (TemplateState& state : templateStates_)
        {
            std::sort(state.record.dependencies.begin(), state.record.dependencies.end());
            state.record.dependencies.erase(
                std::unique(state.record.dependencies.begin(), state.record.dependencies.end()),
                state.record.dependencies.end());
            std::sort(
                state.record.unresolvedDependencies.begin(),
                state.record.unresolvedDependencies.end());
            state.record.unresolvedDependencies.erase(
                std::unique(
                    state.record.unresolvedDependencies.begin(),
                    state.record.unresolvedDependencies.end()),
                state.record.unresolvedDependencies.end());
            std::sort(
                state.record.externalTypeReferences.begin(),
                state.record.externalTypeReferences.end(),
                [](const auto& left, const auto& right) {
                    if (left.range.begin().offset() != right.range.begin().offset())
                        return left.range.begin().offset() < right.range.begin().offset();
                    return left.typeKey < right.typeKey;
                });
            state.record.externalTypeReferences.erase(
                std::unique(
                    state.record.externalTypeReferences.begin(),
                    state.record.externalTypeReferences.end(),
                    [](const auto& left, const auto& right) {
                        return left.range.begin() == right.range.begin() &&
                            left.range.end() == right.range.end() &&
                            left.typeKey == right.typeKey;
                    }),
                state.record.externalTypeReferences.end());
        }
    }

    // LA DETECCION DE DUPLICADOS ES SOLO PARA TIPOS, desde el 2026-09-03.
    //
    // Dos plantillas de METODO con el mismo nombre y la misma aridad generica YA NO son un error:
    // son sobrecargas, y se materializan todas con un nombre compartido para que la resolucion
    // ordinaria elija por tipos (ver processGenericCall). Si despues de sustituir acaban con la
    // MISMA lista de parametros --un duplicado de verdad-- lo detecta la deteccion de metodos
    // duplicados del modelo de miembros, que es donde se comparan listas de parametros.
    //
    // Para clases, structs y delegados no cambia nada: dos con el mismo nombre y aridad siguen
    // siendo un choque, porque ahi no hay lista de parametros que los distinga.
    {
        const GenericTelemetry::Scope phase(telemetry_, "generics.template-index");
        std::unordered_set<std::string> templateKeys;
        for (const TemplateState& state : templateStates_)
        {
            if (state.record.kind == GenericTemplateKind::Method) continue;
            if (!templateKeys.insert(state.record.canonicalKey).second)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::DuplicateSymbol,
                    diagnostics::DiagnosticSeverity::Error,
                    state.syntax != nullptr ? state.syntax->range() : SourceRange{},
                    // Este bucle salta los METODOS --la linea de arriba-- asi que aqui solo llegan
                    // TIPOS genericos, y el mensaje tiene que hablar de tipos. Decia «dos metodos
                    // genericos», que era el texto del caso vecino: un tipo generico no tiene
                    // parametros por los que desambiguar, de modo que dos con el mismo nombre, el
                    // mismo namespace y la misma aridad son sencillamente el mismo tipo dos veces.
                    "duplicate generic type identity '" + state.record.canonicalKey +
                        "'; two generic types in the same namespace cannot share a name and an "
                        "arity -- rename one, or declare it 'partial' at namespace scope");
            }
        }

        templateRecords_.reserve(templateStates_.size());
        for (const TemplateState& state : templateStates_) templateRecords_.push_back(state.record);
    }

    // Seed only identities whose public template was actually hydrated. An old metadata file may
    // list historical specializations without carrying a reusable template body; such a record
    // must never make an unavailable generic appear usable.
    {
        const GenericTelemetry::Scope phase(telemetry_, "generics.imported-specialization-seed");
        for (const PrecompiledGenericSpecialization& imported : precompiledSpecializations)
        {
            if (!imported.hasConcreteDeclaration) continue;
            const auto templateStateIt = stateByCanonicalKey_.find(imported.templateKey);
            if (templateStateIt == stateByCanonicalKey_.end() ||
                specializationPlanner_.byKey.contains(imported.specializationKey))
            {
                continue;
            }
            const TemplateState* templateState = templateStateIt->second;
            const std::size_t ordinal = specializations_.size();
            specializationPlanner_.commit(imported.specializationKey, ordinal);
            specializations_.push_back(GenericSpecializationRecord{
                imported.kind,
                templateState->syntax,
                nullptr,
                imported.templateKey,
                imported.specializationKey,
                imported.specializedName,
                imported.typeArguments,
                ordinal,
                templateState->ordinal,
                nullptr,
                SourceRange{},
                0,
                InstantiationContext{}});
            specializations_.back().state = SpecializationState::Ready;
        }
    }

    {
        const GenericTelemetry::Scope phase(telemetry_, "generics.root-demand-discovery");
        for (const std::unique_ptr<ProjectSyntaxUnit>& unitPtr : units)
        {
            if (unitPtr == nullptr || unitPtr->syntaxTree() == nullptr) continue;
            ProjectSyntaxUnit& unit = *unitPtr;
            processNode(unit, unit.syntaxTree(), "", "", GenericLookupScope{}, false, diagnostics);
        }
    }

    // NSC305-G08. Apply the body-type demands first (they materialize directly and may collect
    // further type demands), then drain the type-demand waves (the two-pass).
    {
        const GenericTelemetry::Scope phase(telemetry_, "generics.body-type-demands");
        if (!materializeBodyTypeSpecializationDemands(diagnostics))
        {
            built_ = true;
            return false;
        }
    }

    telemetry_.maximumLogicalDepth = instantiationBudget_.maximumObservedDepth();
    // NSC305-G-series. Materialization is complete and every rollback has happened, so the
    // specializedName -> ordinal index is final. `emplace` keeps the first entry for a shared
    // overload name, matching `specializationForTypeKey`'s previous forward scan.
    for (std::size_t ordinal = 0; ordinal < specializations_.size(); ++ordinal)
        specializationBySpecializedName_.emplace(specializations_[ordinal].specializedName, ordinal);
    // NSC305-C01. Persist the collected template records (the C1 metadata) when the cache is enabled.
    restoreTemplateRecords();
    storeTemplateRecords();
    built_ = true;
    return !diagnostics.hasErrors();
}

bool ProjectGenericModel::mergeTraitImplementations(
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics)
{
    struct LocatedNode final
    {
        ProjectSyntaxUnit* unit = nullptr;
        ast::AstNode* syntax = nullptr;
        std::string namespaceName;
    };
    std::vector<LocatedNode> templates;
    std::vector<LocatedNode> implementations;
    const auto collect = [&](auto&& self,
                             ProjectSyntaxUnit& unit,
                             ast::AstNode* node,
                             const std::string& parentNamespace) -> void
    {
        if (node == nullptr) return;
        std::string currentNamespace = parentNamespace;
        if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
            currentNamespace = namespaceAppend(parentNamespace, node->text());
        if (node->kind() == ast::AstNodeKind::ClassDeclaration &&
            node->auxiliaryText() == "ns3-trait-impl" &&
            !node->genericParameters().empty())
            implementations.push_back(LocatedNode{&unit, node, currentNamespace});
        else if (node->kind() == ast::AstNodeKind::ClassDeclaration &&
                 !node->genericParameters().empty())
            templates.push_back(LocatedNode{&unit, node, currentNamespace});
        for (ast::AstNode* child : node->children())
            self(self, unit, child, currentNamespace);
    };
    for (const std::unique_ptr<ProjectSyntaxUnit>& unit : units)
        if (unit != nullptr) collect(collect, *unit, unit->syntaxTree(), "");

    const auto parameterShape = [&](const ast::AstNode* node)
    {
        std::string shape;
        for (const ast::GenericParameterSyntax& parameter : node->genericParameterSyntaxes())
        {
            shape += std::to_string(static_cast<unsigned>(parameter.kind));
            shape += ':';
            shape += parameter.name;
            shape += ':';
            shape += typeSpelling(parameter.constantType);
            shape += ';';
        }
        return shape;
    };
    const auto constraintShape = [&](const ast::AstNode* node)
    {
        std::string shape;
        for (const ast::GenericConstraintClause& clause : node->genericConstraints())
        {
            shape += clause.parameterName + ':';
            shape += clause.requiresClass ? 'C' : '-';
            shape += clause.requiresValueType ? 'V' : '-';
            shape += clause.requiresDefaultConstructor ? 'N' : '-';
            shape += clause.requiresCopy ? 'P' : '-';
            shape += clause.requiresMove ? 'M' : '-';
            shape += clause.requiresTrackedLifetime ? 'L' : '-';
            shape += clause.requiresUnmanaged ? 'U' : '-';
            shape += clause.requiresNotNull ? 'R' : '-';
            shape += '=' + typeSpelling(clause.equalityConstraint);
            for (const ast::TypeSyntax* type : clause.typeConstraints)
                shape += ',' + typeSpelling(type);
            shape += ';';
        }
        return shape;
    };

    bool ok = true;
    for (const LocatedNode& implementation : implementations)
    {
        ast::AstNode* impl = implementation.syntax;
        const ast::TypeSyntax* target = impl->second() == nullptr
            ? nullptr : impl->second()->typeSyntax();
        const auto matching = std::ranges::find_if(templates, [&](const LocatedNode& candidate)
        {
            return candidate.unit->projectOrdinal() == implementation.unit->projectOrdinal() &&
                candidate.namespaceName == implementation.namespaceName &&
                candidate.syntax->text() == impl->text() &&
                candidate.syntax->genericParameters().size() == impl->genericParameters().size();
        });
        if (matching == templates.end())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidBaseType,
                diagnostics::DiagnosticSeverity::Error,
                impl->range(),
                "sealed trait coherence rejects this generic impl: target class '" +
                    impl->text() + "' is not declared by this module and namespace");
            ok = false;
            continue;
        }
        ast::AstNode* owner = matching->syntax;
        bool directTarget = target != nullptr &&
            target->typeArguments().size() == impl->genericParameters().size();
        if (directTarget)
        {
            for (std::size_t index = 0; index < target->typeArguments().size(); ++index)
            {
                const ast::TypeSyntax* argument = target->typeArguments()[index];
                directTarget = argument != nullptr && argument->typeArguments().empty() &&
                    argument->modifiers().empty() &&
                    argument->ownershipQualifier() == ast::OwnershipQualifierSyntaxKind::None &&
                    argument->baseName() == impl->genericParameters()[index];
                if (!directTarget) break;
            }
        }
        if (!directTarget || parameterShape(owner) != parameterShape(impl) ||
            constraintShape(owner) != constraintShape(impl))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidBaseType,
                diagnostics::DiagnosticSeverity::Error,
                impl->range(),
                "a conditional trait impl must repeat the target class generic parameters and "
                "where constraints exactly");
            ok = false;
            continue;
        }

        const std::string trait = typeSpelling(impl->typeSyntax());
        const auto duplicate = std::ranges::find_if(owner->typeSyntaxes(), [&](const auto* base)
        {
            return typeSpelling(base) == trait;
        });
        if (duplicate != owner->typeSyntaxes().end())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                impl->typeSyntax()->range(),
                "duplicate trait impl for '" + trait + "' on generic class '" +
                    owner->text() + "'");
            diagnostics.report(
                diagnostics::DiagnosticId::PreviousDeclaration,
                diagnostics::DiagnosticSeverity::Note,
                (*duplicate)->range(),
                "previous trait implementation is here");
            ok = false;
            continue;
        }
        owner->addTypeSyntax(impl->typeSyntax());
        if (owner->typeSyntax() == nullptr) owner->setTypeSyntax(impl->typeSyntax());
        for (ast::AstNode* member : impl->children()) owner->addChild(member);
        traitImplExtensions_[owner].push_back(impl);
    }
    return ok;
}

bool ProjectGenericModel::registerAssociatedItems(
    const ast::AstNode* owner,
    std::string_view ownerName,
    diagnostics::DiagnosticEngine* diagnostics)
{
    if (owner == nullptr || ownerName.empty()) return true;
    bool ok = true;
    for (const ast::AstNode* item : owner->children())
    {
        if (item == nullptr) continue;
        const bool associatedType =
            item->kind() == ast::AstNodeKind::AssociatedTypeDeclaration;
        const bool associatedConstant =
            item->kind() == ast::AstNodeKind::AssociatedConstantDeclaration;
        if ((!associatedType && !associatedConstant) ||
            (associatedType && item->typeSyntax() == nullptr) ||
            (associatedConstant && item->first() == nullptr))
            continue;

        const std::string key = std::string(ownerName) + "." + item->text();
        const ast::AstNode* previous = nullptr;
        if (associatedType)
        {
            const auto [position, inserted] = associatedTypes_.emplace(key, item->typeSyntax());
            if (inserted || position->second == item->typeSyntax()) continue;
            for (const ast::AstNode* candidate : owner->children())
            {
                if (candidate != nullptr && candidate != item &&
                    candidate->kind() == ast::AstNodeKind::AssociatedTypeDeclaration &&
                    candidate->text() == item->text() &&
                    candidate->typeSyntax() == position->second)
                {
                    previous = candidate;
                    break;
                }
            }
        }
        else
        {
            const auto [position, inserted] = associatedConstants_.emplace(key, item);
            if (inserted || position->second == item) continue;
            previous = position->second;
        }

        if (diagnostics != nullptr)
        {
            diagnostics->report(
                diagnostics::DiagnosticId::DuplicateSymbol,
                diagnostics::DiagnosticSeverity::Error,
                item->range(),
                "duplicate associated item '" + key + "'");
            if (previous != nullptr)
            {
                diagnostics->report(
                    diagnostics::DiagnosticId::PreviousDeclaration,
                    diagnostics::DiagnosticSeverity::Note,
                    previous->range(),
                    "previous associated item definition is here");
            }
        }
        ok = false;
    }
    return ok;
}

bool ProjectGenericModel::collectAssociatedItems(
    std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
    diagnostics::DiagnosticEngine& diagnostics)
{
    struct LocatedNode final
    {
        ProjectSyntaxUnit* unit = nullptr;
        ast::AstNode* syntax = nullptr;
        std::string namespaceName;
        std::string ownerName;
    };
    std::vector<LocatedNode> traits;
    std::vector<LocatedNode> owners;
    const auto collect = [&](auto&& self,
                             ast::AstNode* node,
                             const std::string& parentNamespace,
                             const std::string& parentOwner) -> void
    {
        if (node == nullptr) return;
        std::string currentNamespace = parentNamespace;
        if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
            currentNamespace = namespaceAppend(parentNamespace, node->text());
        std::string currentOwner = parentOwner;
        if (node->kind() == ast::AstNodeKind::ClassDeclaration ||
            node->kind() == ast::AstNodeKind::StructDeclaration ||
            node->kind() == ast::AstNodeKind::InterfaceDeclaration)
            currentOwner = namespaceAppend(parentOwner, node->text());
        if (node->kind() == ast::AstNodeKind::InterfaceDeclaration &&
            (node->auxiliaryText() == "ns3-trait" ||
             node->auxiliaryText().find("nsc-meta:ns3-trait") != std::string::npos))
            traits.push_back(LocatedNode{nullptr, node, currentNamespace, currentOwner});
        else if (node->kind() == ast::AstNodeKind::ClassDeclaration ||
                 node->kind() == ast::AstNodeKind::StructDeclaration)
            owners.push_back(LocatedNode{nullptr, node, currentNamespace, currentOwner});
        for (ast::AstNode* child : node->children())
            self(self, child, currentNamespace, currentOwner);
    };
    for (const std::unique_ptr<ProjectSyntaxUnit>& unit : units)
    {
        if (unit == nullptr) continue;
        const std::size_t traitBegin = traits.size();
        const std::size_t ownerBegin = owners.size();
        collect(collect, unit->syntaxTree(), "", "");
        for (std::size_t index = traitBegin; index < traits.size(); ++index)
            traits[index].unit = unit.get();
        for (std::size_t index = ownerBegin; index < owners.size(); ++index)
            owners[index].unit = unit.get();
    }

    std::unordered_map<std::string, const ast::AstNode*> traitTypes;
    std::unordered_map<std::string, const ast::AstNode*> traitConstants;
    std::unordered_map<std::string, const LocatedNode*> traitsByName;
    bool ok = true;
    for (const LocatedNode& trait : traits)
    {
        const std::string traitName = namespaceAppend(
            trait.namespaceName, trait.ownerName);
        traitsByName.emplace(traitName, &trait);
        std::unordered_set<std::string> localNames;
        for (const ast::AstNode* item : trait.syntax->children())
        {
            if (item == nullptr ||
                (item->kind() != ast::AstNodeKind::AssociatedTypeDeclaration &&
                 item->kind() != ast::AstNodeKind::AssociatedConstantDeclaration))
                continue;
            if (!localNames.insert(item->text()).second)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::DuplicateSymbol,
                    diagnostics::DiagnosticSeverity::Error,
                    item->range(),
                    "trait '" + traitName + "' declares associated item '" +
                        item->text() + "' more than once");
                ok = false;
                continue;
            }
            const std::string key = traitName + "." + item->text();
            if (item->kind() == ast::AstNodeKind::AssociatedTypeDeclaration)
                traitTypes.emplace(key, item);
            else
                traitConstants.emplace(key, item);
        }
    }

    for (const LocatedNode& owner : owners)
    {
        const std::string ownerName = namespaceAppend(
            owner.namespaceName, owner.ownerName);
        std::unordered_set<std::string> directTypeKeys;
        std::unordered_set<std::string> directConstantKeys;
        for (const ast::AstNode* item : owner.syntax->children())
        {
            if (item == nullptr) continue;
            const std::string key = ownerName + "." + item->text();
            if (item->kind() == ast::AstNodeKind::AssociatedTypeDeclaration &&
                item->typeSyntax() != nullptr)
                directTypeKeys.insert(key);
            else if (item->kind() == ast::AstNodeKind::AssociatedConstantDeclaration &&
                     item->first() != nullptr)
                directConstantKeys.insert(key);
        }
        ok = registerAssociatedItems(owner.syntax, ownerName, &diagnostics) && ok;

        for (const ast::TypeSyntax* base : owner.syntax->typeSyntaxes())
        {
            if (base == nullptr) continue;
            std::string traitName = base->baseName();
            if (!traitsByName.contains(traitName) &&
                traitName.find('.') == std::string::npos &&
                traitName.find("::") == std::string::npos &&
                !owner.namespaceName.empty())
            {
                const std::string qualified = namespaceAppend(owner.namespaceName, traitName);
                if (traitsByName.contains(qualified)) traitName = qualified;
            }
            const auto traitRecord = traitsByName.find(traitName);
            if (traitRecord == traitsByName.end()) continue;
            std::unordered_map<std::string, const ast::TypeSyntax*> traitSubstitutions;
            const auto& traitParameters = traitRecord->second->syntax->genericParameters();
            const std::size_t substitutedCount = std::min(
                traitParameters.size(), base->typeArguments().size());
            for (std::size_t index = 0; index < substitutedCount; ++index)
                traitSubstitutions.emplace(
                    traitParameters[index], base->typeArguments()[index]);
            const std::string traitPrefix = traitName + ".";
            for (const auto& [key, item] : traitTypes)
            {
                if (!key.starts_with(traitPrefix) || item == nullptr ||
                    item->typeSyntax() == nullptr)
                    continue;
                const std::string ownerKey = ownerName + key.substr(traitName.size());
                const ast::TypeSyntax* defaultType = item->typeSyntax();
                if (!traitSubstitutions.empty() && owner.unit != nullptr)
                    defaultType = cloneType(
                        owner.unit->arena(), item->typeSyntax(), traitSubstitutions);
                const auto [position, inserted] =
                    associatedTypes_.emplace(ownerKey, defaultType);
                if (!inserted && position->second != defaultType &&
                    !directTypeKeys.contains(ownerKey))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        owner.syntax->range(),
                        "associated type '" + ownerKey +
                            "' has competing inherited defaults; define it explicitly");
                    ok = false;
                }
            }
            for (const auto& [key, item] : traitConstants)
            {
                if (!key.starts_with(traitPrefix) || item == nullptr || item->first() == nullptr)
                    continue;
                const std::string ownerKey = ownerName + key.substr(traitName.size());
                const ast::AstNode* defaultConstant = item;
                if (!traitSubstitutions.empty() && owner.unit != nullptr)
                    defaultConstant = cloneNode(
                        owner.unit->arena(), item, traitSubstitutions, "", "");
                const auto [position, inserted] =
                    associatedConstants_.emplace(ownerKey, defaultConstant);
                if (!inserted && position->second != defaultConstant &&
                    !directConstantKeys.contains(ownerKey))
                {
                    diagnostics.report(
                        diagnostics::DiagnosticId::InvalidType,
                        diagnostics::DiagnosticSeverity::Error,
                        owner.syntax->range(),
                        "associated constant '" + ownerKey +
                            "' has competing inherited defaults; define it explicitly");
                    ok = false;
                }
            }

            for (const ast::AstNode* requirement : traitRecord->second->syntax->children())
            {
                if (requirement == nullptr) continue;
                const std::string ownerKey = ownerName + "." + requirement->text();
                if (requirement->kind() == ast::AstNodeKind::AssociatedTypeDeclaration)
                {
                    if (!associatedTypes_.contains(ownerKey))
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidType,
                            diagnostics::DiagnosticSeverity::Error,
                            owner.syntax->range(),
                            "type '" + ownerName + "' does not define required associated type '" +
                                requirement->text() + "' from trait '" + traitName + "'");
                        ok = false;
                    }
                }
                else if (requirement->kind() ==
                         ast::AstNodeKind::AssociatedConstantDeclaration)
                {
                    const auto definition = associatedConstants_.find(ownerKey);
                    if (definition == associatedConstants_.end())
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidType,
                            diagnostics::DiagnosticSeverity::Error,
                            owner.syntax->range(),
                            "type '" + ownerName +
                                "' does not define required associated constant '" +
                                requirement->text() + "' from trait '" + traitName + "'");
                        ok = false;
                        continue;
                    }
                    const ast::AstNode* value = definition->second;
                    const ast::TypeSyntax* requiredType = requirement->typeSyntax();
                    if (requiredType != nullptr && !traitSubstitutions.empty() &&
                        owner.unit != nullptr)
                        requiredType = cloneType(
                            owner.unit->arena(), requiredType, traitSubstitutions);
                    if (value != nullptr && requiredType != nullptr &&
                        value->typeSyntax() != nullptr &&
                        typeSpelling(value->typeSyntax()) != typeSpelling(requiredType))
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidType,
                            diagnostics::DiagnosticSeverity::Error,
                            value->range(),
                            "associated constant '" + ownerKey + "' has type '" +
                                typeSpelling(value->typeSyntax()) + "', but trait '" + traitName +
                                "' requires '" + typeSpelling(requiredType) + "'");
                        ok = false;
                    }
                    if (value != nullptr && value->first() != nullptr &&
                        !ProjectIntegerEvaluator(nullptr).evaluate(value->first()).has_value())
                    {
                        diagnostics.report(
                            diagnostics::DiagnosticId::InvalidType,
                            diagnostics::DiagnosticSeverity::Error,
                            value->first()->range(),
                            "associated constant '" + ownerKey +
                                "' must have a compile-time constant value");
                        ok = false;
                    }
                }
            }
        }
    }

    std::unordered_map<std::string, unsigned> projectionState;
    const auto visitProjection = [&](auto&& self, const std::string& key) -> bool
    {
        unsigned& state = projectionState[key];
        if (state == 2U) return true;
        if (state == 1U)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                associatedTypes_.at(key)->range(),
                "associated type normalization cycle reaches '" + key + "'");
            return false;
        }
        state = 1U;
        const ast::TypeSyntax* definition = associatedTypes_.at(key);
        if (definition != nullptr && associatedTypes_.contains(definition->baseName()) &&
            !self(self, definition->baseName()))
            return false;
        state = 2U;
        return true;
    };
    for (const auto& [key, definition] : associatedTypes_)
        if (definition != nullptr && !visitProjection(visitProjection, key)) ok = false;
    return ok;
}

ast::TypeSyntax* ProjectGenericModel::cloneAliasType(
    ast::AstArena& arena,
    const ast::TypeSyntax* source,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    bool& ownershipConflict)
{
    if (source == nullptr) return nullptr;

    ast::TypeSyntax* result = nullptr;
    const auto substitution = substitutions.find(source->baseName());
    if (substitution != substitutions.end() && source->typeArguments().empty())
    {
        result = cloneAliasType(arena, substitution->second, {}, ownershipConflict);
    }
    else
    {
        result = arena.make<ast::TypeSyntax>(
            source->range(), source->baseTokenKind(), source->baseName());
        result->setTuple(source->isTuple());
        result->setFunction(source->isFunction());
        result->setFunctionCarrier(source->functionCarrier());
        result->setFunctionCallingConvention(source->functionCallingConvention());
        result->setFunctionVariadic(source->functionIsVariadic());
        result->setFunctionUnsafe(source->functionIsUnsafe());
        result->setFunctionNoHeap(source->functionIsNoHeap());
        result->setFunctionAsync(source->functionIsAsync());
        result->setFunctionNullable(source->functionIsNullable());
        result->setFunctionLifetimeName(source->functionLifetimeName());
        result->setFunctionReturnType(cloneAliasType(
            arena, source->functionReturnType(), substitutions, ownershipConflict));
        result->setFunctionErrorType(cloneAliasType(
            arena, source->functionErrorType(), substitutions, ownershipConflict));
        for (std::size_t index = 0; index < source->typeArguments().size(); ++index)
        {
            ast::TypeSyntax* argument = cloneAliasType(
                arena, source->typeArguments()[index], substitutions, ownershipConflict);
            if (source->isTuple())
            {
                const std::string name = index < source->tupleElementNames().size()
                    ? source->tupleElementNames()[index] : std::string{};
                result->addTupleElement(argument, name);
            }
            else
            {
                result->addTypeArgument(argument);
            }
        }
        if (source->originProjectOrdinal().has_value())
            result->setOriginProjectOrdinal(source->originProjectOrdinal().value());
        if (source->originNamespace().has_value())
            result->setOriginNamespace(source->originNamespace().value());
    }

    if (source->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None)
    {
        if (result->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None &&
            result->ownershipQualifier() != source->ownershipQualifier())
            ownershipConflict = true;
        else
            result->setOwnershipQualifier(source->ownershipQualifier());
    }
    result->setLifetimeName(source->lifetimeName());
    for (const ast::TypeModifierSyntax& modifier : source->modifiers())
    {
        // Array extents are immutable expression syntax owned by the same source unit.  Alias
        // substitution changes types, never value parameters, so sharing this pointer is exact.
        result->addModifier(ast::TypeModifierSyntax{
            modifier.kind, modifier.arrayCount, modifier.range});
    }
    result->setPackExpansion(source->isPackExpansion());
    result->setDynamicTrait(source->isDynamicTrait());
    result->setOpaqueResult(source->isOpaqueResult());
    result->setOpaqueResultTraitName(source->opaqueResultTraitName());
    result->setPointeeConst(source->isPointeeConst());
    return result;
}

bool ProjectGenericModel::expandTypeAliases(
    ProjectSyntaxUnit& unit,
    ast::TypeSyntax* syntax,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics,
    std::vector<std::string>& expansionStack)
{
    if (syntax == nullptr) return true;
    std::string constantType;
    std::string constantValue;
    if (decodeConstGenericArgument(syntax, constantType, constantValue)) return true;
    bool ok = true;
    for (ast::TypeSyntax* argument : syntax->typeArguments())
        ok = expandTypeAliases(unit, argument, scope, diagnostics, expansionStack) && ok;

    if (lexer::isBuiltinTypeKeyword(syntax->baseTokenKind()) ||
        scope.typeParameters.read().contains(syntax->baseName()))
        return ok;

    const auto found = scope.typeAliases.read().find(syntax->baseName());
    if (found == scope.typeAliases.read().end() || found->second == nullptr) return ok;
    const ast::AstNode* declaration = found->second;

    if (std::find(expansionStack.begin(), expansionStack.end(), syntax->baseName()) !=
        expansionStack.end())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidUsingTarget,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "using alias '" + syntax->baseName() + "' expands cyclically");
        return false;
    }

    if (syntax->typeArguments().size() != declaration->genericParameters().size())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "type alias '" + declaration->text() + "' expects " +
                std::to_string(declaration->genericParameters().size()) +
                " type argument(s) but received " +
                std::to_string(syntax->typeArguments().size()));
        return false;
    }

    std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
    for (std::size_t index = 0; index < declaration->genericParameters().size(); ++index)
        substitutions.emplace(declaration->genericParameters()[index], syntax->typeArguments()[index]);

    expansionStack.push_back(syntax->baseName());
    bool targetOwnershipConflict = false;
    ast::TypeSyntax* expanded =
        cloneAliasType(
            unit.arena(), declaration->typeSyntax(), substitutions, targetOwnershipConflict);
    if (targetOwnershipConflict)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "type alias '" + declaration->text() +
                "' applies a conflicting ownership qualifier to a type argument");
        expansionStack.pop_back();
        return false;
    }
    ok = expandTypeAliases(unit, expanded, scope, diagnostics, expansionStack) && ok;
    expansionStack.pop_back();
    if (!ok || expanded == nullptr) return false;

    const ast::OwnershipQualifierSyntaxKind writtenOwnership = syntax->ownershipQualifier();
    const ast::OwnershipQualifierSyntaxKind targetOwnership = expanded->ownershipQualifier();
    if (writtenOwnership != ast::OwnershipQualifierSyntaxKind::None &&
        targetOwnership != ast::OwnershipQualifierSyntaxKind::None &&
        writtenOwnership != targetOwnership)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "type alias '" + declaration->text() +
                "' conflicts with the ownership qualifier written at its use site");
        return false;
    }

    syntax->setBaseTokenKind(expanded->baseTokenKind());
    syntax->setBaseName(expanded->baseName());
    syntax->setDynamicTrait(syntax->isDynamicTrait() || expanded->isDynamicTrait());
    syntax->setOpaqueResult(syntax->isOpaqueResult() || expanded->isOpaqueResult());
    if (syntax->opaqueResultTraitName().empty())
        syntax->setOpaqueResultTraitName(expanded->opaqueResultTraitName());
    if (writtenOwnership == ast::OwnershipQualifierSyntaxKind::None)
        syntax->setOwnershipQualifier(targetOwnership);
    const std::vector<ast::TypeSyntax*> expandedArguments(
        expanded->typeArguments().begin(), expanded->typeArguments().end());
    syntax->clearTypeArguments();
    for (ast::TypeSyntax* argument : expandedArguments) syntax->addTypeArgument(argument);
    // Target/substituted modifiers are inside modifiers written on the alias use.  For example,
    // `using P<T> = T*; P<int>[]` becomes `int*[]`.
    syntax->prependModifiers(expanded->modifiers());
    if (expanded->originProjectOrdinal().has_value())
        syntax->setOriginProjectOrdinal(expanded->originProjectOrdinal().value());
    if (expanded->originNamespace().has_value())
        syntax->setOriginNamespace(expanded->originNamespace().value());
    return true;
}

bool ProjectGenericModel::expandAliasesInNode(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    std::string namespaceName,
    GenericLookupScope scope,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // NSC305-G05. Iterative pre-order traversal with an explicit stack; the per-node logic is
    // unchanged, only the recursion into first/second/third/children is removed.
    struct AliasFrame
    {
        ast::AstNode* node;
        std::string namespaceName;
        GenericLookupScope scope;
    };
    std::vector<AliasFrame> frames;
    frames.push_back(AliasFrame{node, std::move(namespaceName), std::move(scope)});
    bool ok = true;
    while (!frames.empty())
    {
        AliasFrame frame = std::move(frames.back());
        frames.pop_back();
        node = frame.node;
        namespaceName = std::move(frame.namespaceName);
        scope = std::move(frame.scope);
        if (node == nullptr) continue;
    std::string currentNamespace = namespaceName;
    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
        currentNamespace = namespaceAppend(namespaceName, node->text());

    GenericLookupScope currentScope = scope;
    if (node->kind() == ast::AstNodeKind::CompilationUnit ||
        node->kind() == ast::AstNodeKind::NamespaceDeclaration)
        currentScope.declarationContainer = node;
    for (const ast::GenericParameterSyntax& parameter : node->genericParameterSyntaxes())
        if (parameter.kind == ast::GenericParameterSyntaxKind::Type ||
            parameter.kind == ast::GenericParameterSyntaxKind::TypePack)
            currentScope.typeParameters.write().insert(parameter.name);
    for (const ast::AstNode* child : node->children())
    {
        if (child == nullptr || child->kind() != ast::AstNodeKind::UsingAliasDirective) continue;
        if (!child->genericParameters().empty() || child->auxiliaryText().empty())
            currentScope.typeAliases.write()[child->text()] = child;
    }

    bool nodeOk = true;
    // Normalize every alias target in its declaration scope first.  This makes a chained alias
    // immune to a same-named alias that may shadow it only at a later use site.
    for (const ast::AstNode* child : node->children())
    {
        if (child == nullptr || child->kind() != ast::AstNodeKind::UsingAliasDirective ||
            child->typeSyntax() == nullptr ||
            (child->genericParameters().empty() && !child->auxiliaryText().empty()))
            continue;
        GenericLookupScope aliasScope = currentScope;
        for (const ast::GenericParameterSyntax& parameter : child->genericParameterSyntaxes())
            if (parameter.kind == ast::GenericParameterSyntaxKind::Type ||
                parameter.kind == ast::GenericParameterSyntaxKind::TypePack)
                aliasScope.typeParameters.write().insert(parameter.name);
        std::vector<std::string> stack{child->text()};
        nodeOk = expandTypeAliases(
                 unit, child->typeSyntax(), aliasScope, diagnostics, stack) && nodeOk;
    }

    if (node->kind() != ast::AstNodeKind::UsingAliasDirective)
    {
        std::vector<std::string> stack;
        nodeOk = expandTypeAliases(unit, node->typeSyntax(), currentScope, diagnostics, stack) && nodeOk;
        for (ast::TypeSyntax* syntax : node->typeSyntaxes())
        {
            stack.clear();
            nodeOk = expandTypeAliases(unit, syntax, currentScope, diagnostics, stack) && nodeOk;
        }
        for (const ast::GenericConstraintClause& clause : node->genericConstraints())
        {
            for (ast::TypeSyntax* constraint : clause.typeConstraints)
            {
                stack.clear();
                nodeOk = expandTypeAliases(unit, constraint, currentScope, diagnostics, stack) && nodeOk;
            }
            if (clause.equalityConstraint != nullptr)
            {
                stack.clear();
                nodeOk = expandTypeAliases(
                    unit, clause.equalityConstraint, currentScope, diagnostics, stack) && nodeOk;
            }
        }
    }

    ok = nodeOk && ok;
    const std::vector<ast::AstNode*> children(node->children().begin(), node->children().end());
    for (auto iterator = children.rbegin(); iterator != children.rend(); ++iterator)
        frames.push_back(AliasFrame{*iterator, currentNamespace, currentScope});
    if (node->third() != nullptr)
        frames.push_back(AliasFrame{node->third(), currentNamespace, currentScope});
    if (node->second() != nullptr)
        frames.push_back(AliasFrame{node->second(), currentNamespace, currentScope});
    if (node->first() != nullptr)
        frames.push_back(AliasFrame{node->first(), currentNamespace, currentScope});
    }
    return ok;
}

std::span<const GenericTemplateRecord> ProjectGenericModel::templates() const noexcept
{
    return templateRecords_;
}

std::span<const GenericSpecializationRecord> ProjectGenericModel::specializations() const noexcept
{
    return specializations_;
}

const GenericSpecializationRecord* ProjectGenericModel::specializationForSyntax(
    const ast::AstNode* syntax) const noexcept
{
    const auto found = specializationBySyntax_.find(syntax);
    return found != specializationBySyntax_.end() && found->second < specializations_.size()
        ? &specializations_[found->second] : nullptr;
}

const GenericSpecializationRecord* ProjectGenericModel::specializationForTypeKey(
    std::string_view typeKey) const noexcept
{
    if (typeKey.starts_with("type:")) typeKey.remove_prefix(5U);
    const std::size_t separator = typeKey.find_last_of(".+:");
    const std::string_view terminal = separator == std::string_view::npos
        ? typeKey : typeKey.substr(separator + 1U);
    const auto found = specializationBySpecializedName_.find(std::string(terminal));
    return found != specializationBySpecializedName_.end() &&
            found->second < specializations_.size()
        ? &specializations_[found->second] : nullptr;
}

std::string ProjectGenericModel::closedTypeKeyForTemplate(
    std::string_view templateName,
    std::string_view elementTypeKey) const
{
    const GenericTemplateRecord* templateRecord = nullptr;
    for (const GenericTemplateRecord& record : templates())
    {
        if (record.name == templateName)
        {
            templateRecord = &record;
            break;
        }
    }
    if (templateRecord == nullptr) return {};

    // Keep the terminal spelling only for the readable generated symbol. The hash must use the
    // same declaration-qualified identity as ordinary generic specialization; otherwise two
    // modules or namespaces that both declare `Item` synthesize the same Span<Item> type key.
    std::string spelling = std::string(elementTypeKey);
    const bool builtin = spelling.starts_with("builtin:");
    if (builtin) spelling.erase(0, 8U);
    if (spelling.starts_with("type:")) spelling.erase(0, 5U);
    const std::size_t separator = spelling.find_last_of(".+:");
    if (separator != std::string::npos) spelling.erase(0, separator + 1U);
    std::string identity;
    if (builtin)
    {
        identity = spelling;
    }
    else if (const GenericSpecializationRecord* specialization =
                 specializationForTypeKey(elementTypeKey);
             specialization != nullptr)
    {
        identity = "generic:" + specialization->specializationKey;
    }
    else
    {
        identity = std::string(elementTypeKey);
    }

    const std::string key = templateRecord->canonicalKey + "<" + identity + ">";
    const std::string specializedName = templateRecord->name + "__g1__" + sanitize(spelling) +
        "__h" + stableHashSuffix(key);

    const std::string_view canonicalKey = templateRecord->canonicalKey;
    const std::size_t moduleSeparator = canonicalKey.find("::");
    if (moduleSeparator == std::string_view::npos) return {};
    const std::size_t terminal = canonicalKey.find_last_of(".+");
    const std::size_t terminalBegin = terminal == std::string_view::npos ||
            terminal < moduleSeparator + 2U
        ? moduleSeparator + 2U
        : terminal + 1U;
    if (canonicalKey.find('`', terminalBegin) == std::string_view::npos) return {};
    return "type:" + std::string(canonicalKey.substr(0, terminalBegin)) + specializedName;
}

const std::string* ProjectGenericModel::resolvedTypeIdentity(
    const ast::TypeSyntax* syntax) const noexcept
{
    const auto found = resolvedTypeIdentityBySyntax_.find(syntax);
    return found == resolvedTypeIdentityBySyntax_.end() ? nullptr : &found->second;
}

GenericInstantiationReport ProjectGenericModel::instantiationReport() const noexcept
{
    return GenericInstantiationReport{
        instantiationBudget_.count(),
        instantiationCacheHitCount_,
        instantiationBudget_.maximumObservedDepth(),
        instantiationBudget_.maximumDepth(),
        instantiationBudget_.maximumCount()};
}

GenericTelemetry& ProjectGenericModel::telemetry() noexcept
{
    return telemetry_;
}

const GenericTelemetry& ProjectGenericModel::telemetry() const noexcept
{
    return telemetry_;
}

std::string GenericTelemetry::renderSummary() const
{
    // Fixed order keeps the summary deterministic regardless of unordered_map iteration.
    static constexpr std::string_view subPhaseNames[] = {
        "generics.visibility",
        "generics.alias-expansion",
        "generics.trait-merge",
        "generics.opaque-results",
        "generics.ordinary-return-index",
        "generics.associated-items",
        "generics.template-collection",
        "generics.template-index",
        "generics.dependency-discovery",
        "generics.imported-specialization-seed",
        "generics.root-demand-discovery",
        "generics.specialization-waves",
        "generics.type-demand-waves",
        "generics.rewrite-commit",
        "generics.body-type-demands",
    };
    std::ostringstream output;
    output << "generics.telemetry enabled=" << (enabled ? 1 : 0)
           << " detailed=" << (detailed ? 1 : 0)
           << " own-units=" << ownUnits
           << " template-source-bytes=" << templateSourceBytes
           << " templates=" << templatesCollected
           << " resolution-queries=" << resolutionQueries
           << " candidates-examined=" << candidatesExamined
           << " demands-discovered=" << demandsDiscovered
           << " demands-duplicated=" << demandsDuplicated
           << " demands-materialized=" << demandsMaterialized
           << " specialization-hits=" << specializationHits
           << " nodes-cloned=" << nodesCloned
           << " types-cloned=" << typesCloned
           << " maximum-logical-depth=" << maximumLogicalDepth
           << " scope-copies=" << scopeCopies
           << " scope-copied-entries=" << scopeCopiedEntries << '\n';
    for (const std::string_view name : subPhaseNames)
    {
        const auto found = subPhaseMicros.find(std::string(name));
        output << "subphase " << name << " us="
               << (found == subPhaseMicros.end() ? 0U : found->second) << '\n';
    }
    // Per-template detail uses canonical-key order so the report is reproducible.
    std::vector<std::pair<std::string, TemplateDetail>> sortedTemplates(
        templates.begin(), templates.end());
    std::sort(sortedTemplates.begin(), sortedTemplates.end(),
        [](const auto& left, const auto& right) { return left.first < right.first; });
    for (const auto& [key, detail] : sortedTemplates)
    {
        output << "template-detail key=" << key
               << " materializations=" << detail.materializations
               << " exclusive_us=" << detail.exclusiveMicros << '\n';
    }
    return output.str();
}

bool ProjectGenericModel::isBuilt() const noexcept
{
    return built_;
}

void ProjectGenericModel::setTemplateCacheRoot(std::filesystem::path root)
{
    templateCache_.setRoot(std::move(root));
}

// NSC305-C01. SHA-256 of a template's source text (reconstructed from the lexer tokens), the C1
// cache key input that invalidates on any source change (including the body).
static std::string computeTemplateSourceFingerprint(
    const ProjectSyntaxUnit& unit, const ast::AstNode* node)
{
    std::string canonical;
    const std::uint32_t begin = node->range().begin().offset();
    const std::uint32_t end = node->range().end().offset();
    for (const lexer::Token& token : unit.tokens())
    {
        const std::uint32_t tokenBegin = token.range().begin().offset();
        const std::uint32_t tokenEnd = token.range().end().offset();
        if (tokenBegin >= begin && tokenEnd <= end)
        {
            canonical += token.lexeme();
            canonical.push_back(' ');
        }
    }
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(canonical.data()), canonical.size()};
    return basic::sha256Hex(basic::sha256(bytes));
}

// NSC305-C01. The C1 cache schema version (bump on any format change).
static constexpr std::string_view templateCacheSchema = "nsc-c1-template-v1";

// NSC305-C01. The full C1 cache key: SHA-256 of (source fingerprint + schema version).
static std::string templateCacheKey(std::string_view sourceFingerprint)
{
    std::string material{sourceFingerprint};
    material.push_back('|');
    material += templateCacheSchema;
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(material.data()), material.size()};
    return basic::sha256Hex(basic::sha256(bytes));
}

// NSC305-C01. Compact, relocatable (no raw pointers) serialization of the scalar/string metadata of
// a GenericTemplateRecord -- the C1 cache payload skeleton. The AST subtree (syntax/extensionSyntaxes,
// parameterSyntaxes, constraints, externalTypeReferences) is intentionally not covered here; it is
// the next layer of the codec.
namespace
{
void appendByte(std::string& out, std::uint8_t value)
{
    out.push_back(static_cast<char>(value));
}

void appendU32(std::string& out, std::uint32_t value)
{
    out.push_back(static_cast<char>(value & 0xFFu));
    out.push_back(static_cast<char>((value >> 8U) & 0xFFu));
    out.push_back(static_cast<char>((value >> 16U) & 0xFFu));
    out.push_back(static_cast<char>((value >> 24U) & 0xFFu));
}

void appendU64(std::string& out, std::uint64_t value)
{
    for (unsigned shift = 0; shift < 64U; shift += 8U)
        out.push_back(static_cast<char>((value >> shift) & 0xFFu));
}

void appendString(std::string& out, std::string_view value)
{
    appendU32(out, static_cast<std::uint32_t>(value.size()));
    out.append(value);
}

void appendStringVector(std::string& out, const std::vector<std::string>& values)
{
    appendU32(out, static_cast<std::uint32_t>(values.size()));
    for (const std::string& value : values) appendString(out, value);
}

class ByteReader final
{
public:
    explicit ByteReader(std::string_view bytes) : bytes_(bytes) {}

    bool byte(std::uint8_t& out)
    {
        if (position_ >= bytes_.size()) return false;
        out = static_cast<std::uint8_t>(bytes_[position_++]);
        return true;
    }
    bool u32(std::uint32_t& out)
    {
        out = 0;
        for (unsigned shift = 0; shift < 32U; shift += 8U)
        {
            std::uint8_t value = 0;
            if (!byte(value)) return false;
            out |= static_cast<std::uint32_t>(value) << shift;
        }
        return true;
    }
    bool u64(std::uint64_t& out)
    {
        out = 0;
        for (unsigned shift = 0; shift < 64U; shift += 8U)
        {
            std::uint8_t value = 0;
            if (!byte(value)) return false;
            out |= static_cast<std::uint64_t>(value) << shift;
        }
        return true;
    }
    bool string(std::string& out)
    {
        std::uint32_t size = 0;
        if (!u32(size)) return false;
        if (position_ + size > bytes_.size()) return false;
        out.assign(bytes_.data() + position_, size);
        position_ += size;
        return true;
    }
    bool stringVector(std::vector<std::string>& out)
    {
        std::uint32_t count = 0;
        if (!u32(count)) return false;
        out.clear();
        out.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            std::string value;
            if (!string(value)) return false;
            out.push_back(std::move(value));
        }
        return true;
    }

private:
    std::string_view bytes_;
    std::size_t position_ = 0;
};

[[maybe_unused]] std::string serializeTemplateRecord(const GenericTemplateRecord& record)
{
    std::string out;
    appendByte(out, static_cast<std::uint8_t>(record.kind));
    appendByte(out, static_cast<std::uint8_t>(record.accessibility));
    appendU64(out, record.projectOrdinal);
    appendU64(out, record.sourceOrdinal);
    appendString(out, record.moduleName);
    appendString(out, record.namespaceName);
    appendString(out, record.ownerName);
    appendString(out, record.name);
    appendString(out, record.qualifiedTypeName);
    appendStringVector(out, record.parameters);
    appendString(out, record.canonicalKey);
    appendString(out, record.specializationFamilyKey);
    appendByte(out, record.isControlledSpecialization ? 1U : 0U);
    appendByte(out, record.isControlledSpecializationFamily ? 1U : 0U);
    appendStringVector(out, record.dependencies);
    appendStringVector(out, record.unresolvedDependencies);
    appendString(out, record.sourceFingerprint);
    appendString(out, record.c1CacheKey);
    return out;
}

// NSC305-C01. ByteReader-based record metadata read (shared by deserializeTemplateRecord and the
// C02 catalog restore).
[[maybe_unused]] bool readTemplateRecord(ByteReader& reader, GenericTemplateRecord& record)
{
    std::uint8_t kind = 0;
    std::uint8_t accessibility = 0;
    std::uint64_t projectOrdinal = 0;
    std::uint64_t sourceOrdinal = 0;
    std::uint8_t controlled = 0;
    std::uint8_t controlledFamily = 0;
    if (!reader.byte(kind) || !reader.byte(accessibility) ||
        !reader.u64(projectOrdinal) || !reader.u64(sourceOrdinal) ||
        !reader.string(record.moduleName) || !reader.string(record.namespaceName) ||
        !reader.string(record.ownerName) || !reader.string(record.name) ||
        !reader.string(record.qualifiedTypeName) ||
        !reader.stringVector(record.parameters) || !reader.string(record.canonicalKey) ||
        !reader.string(record.specializationFamilyKey) ||
        !reader.byte(controlled) || !reader.byte(controlledFamily) ||
        !reader.stringVector(record.dependencies) ||
        !reader.stringVector(record.unresolvedDependencies) ||
        !reader.string(record.sourceFingerprint) || !reader.string(record.c1CacheKey))
        return false;
    record.kind = static_cast<GenericTemplateKind>(kind);
    record.accessibility = static_cast<GenericTemplateAccessibility>(accessibility);
    record.projectOrdinal = static_cast<std::size_t>(projectOrdinal);
    record.sourceOrdinal = static_cast<std::size_t>(sourceOrdinal);
    record.isControlledSpecialization = controlled != 0U;
    record.isControlledSpecializationFamily = controlledFamily != 0U;
    return true;
}

[[maybe_unused]] bool deserializeTemplateRecord(std::string_view bytes, GenericTemplateRecord& record)
{
    ByteReader reader(bytes);
    return readTemplateRecord(reader, record);
}

// NSC305-C01. Scalar/string fields of a TypeSyntax (no pointers, no recursion): the base identity
// plus the source flags/origin tags. The type-argument/return/error/modifier graph is the next layer.
[[maybe_unused]] std::string serializeTypeSyntaxScalars(const ast::TypeSyntax& type)
{
    std::string out;
    appendU32(out, type.range().begin().offset());
    appendU32(out, type.range().end().offset());
    appendU32(out, static_cast<std::uint32_t>(type.baseTokenKind()));
    appendString(out, type.baseName());
    appendByte(out, static_cast<std::uint8_t>(type.ownershipQualifier()));
    appendByte(out, static_cast<std::uint8_t>(type.referenceQualifier()));
    appendString(out, type.lifetimeName());
    appendByte(out, type.isDynamicTrait() ? 1U : 0U);
    appendByte(out, type.isOpaqueResult() ? 1U : 0U);
    appendString(out, type.opaqueResultTraitName());
    appendByte(out, type.isExplicitTraitImplementation() ? 1U : 0U);
    appendByte(out, type.isPointeeConst() ? 1U : 0U);
    appendByte(out, type.isTuple() ? 1U : 0U);
    appendByte(out, type.isFunction() ? 1U : 0U);
    appendByte(out, type.isPackExpansion() ? 1U : 0U);
    appendByte(out, static_cast<std::uint8_t>(type.functionCarrier()));
    appendByte(out, static_cast<std::uint8_t>(type.functionCallingConvention()));
    appendByte(out, type.functionIsVariadic() ? 1U : 0U);
    appendByte(out, type.functionIsUnsafe() ? 1U : 0U);
    appendByte(out, type.functionIsNoHeap() ? 1U : 0U);
    appendByte(out, type.functionIsAsync() ? 1U : 0U);
    appendByte(out, type.functionIsNullable() ? 1U : 0U);
    appendString(out, type.functionLifetimeName());
    appendStringVector(out, type.tupleElementNames());
    appendByte(out, type.originProjectOrdinal().has_value() ? 1U : 0U);
    if (type.originProjectOrdinal().has_value())
        appendU64(out, type.originProjectOrdinal().value());
    appendByte(out, type.originNamespace().has_value() ? 1U : 0U);
    if (type.originNamespace().has_value())
        appendString(out, type.originNamespace().value());
    return out;
}

// NSC305-C01. Deserializes the scalar/string fields written by serializeTypeSyntaxScalars. The
// SourceRange's FileId is left default (invalid); the caller restores it from the owning unit.
[[maybe_unused]] bool deserializeTypeSyntaxScalars(
    std::string_view bytes, ast::TypeSyntax& type)
{
    ByteReader reader(bytes);
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
    std::uint32_t tokenKind = 0;
    std::string baseName;
    std::uint8_t ownership = 0;
    std::uint8_t reference = 0;
    std::string lifetimeName;
    std::uint8_t dynamicTrait = 0;
    std::uint8_t opaqueResult = 0;
    std::string opaqueResultTraitName;
    std::uint8_t explicitTrait = 0;
    std::uint8_t pointeeConst = 0;
    std::uint8_t tuple = 0;
    std::uint8_t function = 0;
    std::uint8_t pack = 0;
    std::uint8_t carrier = 0;
    std::uint8_t convention = 0;
    std::uint8_t variadic = 0;
    std::uint8_t unsafe = 0;
    std::uint8_t noHeap = 0;
    std::uint8_t async = 0;
    std::uint8_t nullable = 0;
    std::string functionLifetimeName;
    std::vector<std::string> tupleNames;
    std::uint8_t hasProject = 0;
    std::uint64_t project = 0;
    std::uint8_t hasNamespace = 0;
    std::string originNamespace;
    if (!reader.u32(begin) || !reader.u32(end) || !reader.u32(tokenKind) ||
        !reader.string(baseName) || !reader.byte(ownership) || !reader.byte(reference) ||
        !reader.string(lifetimeName) || !reader.byte(dynamicTrait) || !reader.byte(opaqueResult) ||
        !reader.string(opaqueResultTraitName) || !reader.byte(explicitTrait) ||
        !reader.byte(pointeeConst) || !reader.byte(tuple) || !reader.byte(function) ||
        !reader.byte(pack) || !reader.byte(carrier) || !reader.byte(convention) ||
        !reader.byte(variadic) || !reader.byte(unsafe) || !reader.byte(noHeap) ||
        !reader.byte(async) || !reader.byte(nullable) || !reader.string(functionLifetimeName) ||
        !reader.stringVector(tupleNames) || !reader.byte(hasProject) ||
        (hasProject != 0U && !reader.u64(project)) || !reader.byte(hasNamespace) ||
        (hasNamespace != 0U && !reader.string(originNamespace)))
        return false;
    type.setRange(SourceRange{SourceLocation{FileId{}, begin}, SourceLocation{FileId{}, end}});
    type.setBaseTokenKind(static_cast<lexer::TokenKind>(tokenKind));
    type.setBaseName(std::move(baseName));
    type.setOwnershipQualifier(static_cast<ast::OwnershipQualifierSyntaxKind>(ownership));
    type.setReferenceQualifier(static_cast<ast::ReferenceQualifierSyntaxKind>(reference));
    type.setLifetimeName(std::move(lifetimeName));
    type.setDynamicTrait(dynamicTrait != 0U);
    type.setOpaqueResult(opaqueResult != 0U);
    type.setOpaqueResultTraitName(std::move(opaqueResultTraitName));
    type.setExplicitTraitImplementation(explicitTrait != 0U);
    type.setPointeeConst(pointeeConst != 0U);
    type.setTuple(tuple != 0U);
    type.setFunction(function != 0U);
    type.setPackExpansion(pack != 0U);
    type.setFunctionCarrier(static_cast<ast::FunctionTypeCarrierSyntaxKind>(carrier));
    type.setFunctionCallingConvention(
        static_cast<ast::FunctionCallingConventionSyntaxKind>(convention));
    type.setFunctionVariadic(variadic != 0U);
    type.setFunctionUnsafe(unsafe != 0U);
    type.setFunctionNoHeap(noHeap != 0U);
    type.setFunctionAsync(async != 0U);
    type.setFunctionNullable(nullable != 0U);
    type.setFunctionLifetimeName(std::move(functionLifetimeName));
    // Tuple element names are restored by the recursive layer alongside the elements.
    (void)tupleNames;
    if (hasProject != 0U) type.setOriginProjectOrdinal(static_cast<std::size_t>(project));
    if (hasNamespace != 0U) type.setOriginNamespace(std::move(originNamespace));
    return true;
}

// NSC305-C01. Recursive (tree) serialization of a TypeSyntax: the scalar fields plus the ordered
// type arguments and the function return/error types. Modifiers (arrayCount expressions) and
// constExpression are the AstNode layer.
[[maybe_unused]] std::string serializeTypeSyntaxTree(const ast::TypeSyntax* type)
{
    if (type == nullptr) return {};
    std::string out = serializeTypeSyntaxScalars(*type);
    appendU32(out, static_cast<std::uint32_t>(type->typeArguments().size()));
    for (const ast::TypeSyntax* argument : type->typeArguments())
        out += serializeTypeSyntaxTree(argument);
    const bool hasReturn = type->functionReturnType() != nullptr;
    const bool hasError = type->functionErrorType() != nullptr;
    appendByte(out, hasReturn ? 1U : 0U);
    appendByte(out, hasError ? 1U : 0U);
    if (hasReturn) out += serializeTypeSyntaxTree(type->functionReturnType());
    if (hasError) out += serializeTypeSyntaxTree(type->functionErrorType());
    return out;
}

// NSC305-C01. Recursive deserialization of a TypeSyntax tree into `arena` (inverse of
// serializeTypeSyntaxTree). The SourceRange's FileId is left default (invalid); the caller restores
// it from the owning unit.
[[maybe_unused]] ast::TypeSyntax* deserializeTypeSyntaxTree(
    ast::AstArena& arena, ByteReader& reader)
{
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
    std::uint32_t tokenKind = 0;
    std::string baseName;
    if (!reader.u32(begin) || !reader.u32(end) || !reader.u32(tokenKind) ||
        !reader.string(baseName))
        return nullptr;
    ast::TypeSyntax* type = arena.make<ast::TypeSyntax>(
        SourceRange{SourceLocation{FileId{}, begin}, SourceLocation{FileId{}, end}},
        static_cast<lexer::TokenKind>(tokenKind), std::move(baseName));
    std::uint8_t ownership = 0;
    std::uint8_t reference = 0;
    std::string lifetimeName;
    std::uint8_t dynamicTrait = 0;
    std::uint8_t opaqueResult = 0;
    std::string opaqueResultTraitName;
    std::uint8_t explicitTrait = 0;
    std::uint8_t pointeeConst = 0;
    std::uint8_t tuple = 0;
    std::uint8_t function = 0;
    std::uint8_t pack = 0;
    std::uint8_t carrier = 0;
    std::uint8_t convention = 0;
    std::uint8_t variadic = 0;
    std::uint8_t unsafe = 0;
    std::uint8_t noHeap = 0;
    std::uint8_t async = 0;
    std::uint8_t nullable = 0;
    std::string functionLifetimeName;
    std::vector<std::string> tupleNames;
    std::uint8_t hasProject = 0;
    std::uint64_t project = 0;
    std::uint8_t hasNamespace = 0;
    std::string originNamespace;
    if (!reader.byte(ownership) || !reader.byte(reference) || !reader.string(lifetimeName) ||
        !reader.byte(dynamicTrait) || !reader.byte(opaqueResult) ||
        !reader.string(opaqueResultTraitName) || !reader.byte(explicitTrait) ||
        !reader.byte(pointeeConst) || !reader.byte(tuple) || !reader.byte(function) ||
        !reader.byte(pack) || !reader.byte(carrier) || !reader.byte(convention) ||
        !reader.byte(variadic) || !reader.byte(unsafe) || !reader.byte(noHeap) ||
        !reader.byte(async) || !reader.byte(nullable) || !reader.string(functionLifetimeName) ||
        !reader.stringVector(tupleNames) || !reader.byte(hasProject) ||
        (hasProject != 0U && !reader.u64(project)) || !reader.byte(hasNamespace) ||
        (hasNamespace != 0U && !reader.string(originNamespace)))
        return nullptr;
    type->setOwnershipQualifier(static_cast<ast::OwnershipQualifierSyntaxKind>(ownership));
    type->setReferenceQualifier(static_cast<ast::ReferenceQualifierSyntaxKind>(reference));
    type->setLifetimeName(std::move(lifetimeName));
    type->setDynamicTrait(dynamicTrait != 0U);
    type->setOpaqueResult(opaqueResult != 0U);
    type->setOpaqueResultTraitName(std::move(opaqueResultTraitName));
    type->setExplicitTraitImplementation(explicitTrait != 0U);
    type->setPointeeConst(pointeeConst != 0U);
    type->setTuple(tuple != 0U);
    type->setFunction(function != 0U);
    type->setPackExpansion(pack != 0U);
    type->setFunctionCarrier(static_cast<ast::FunctionTypeCarrierSyntaxKind>(carrier));
    type->setFunctionCallingConvention(
        static_cast<ast::FunctionCallingConventionSyntaxKind>(convention));
    type->setFunctionVariadic(variadic != 0U);
    type->setFunctionUnsafe(unsafe != 0U);
    type->setFunctionNoHeap(noHeap != 0U);
    type->setFunctionAsync(async != 0U);
    type->setFunctionNullable(nullable != 0U);
    type->setFunctionLifetimeName(std::move(functionLifetimeName));
    if (hasProject != 0U) type->setOriginProjectOrdinal(static_cast<std::size_t>(project));
    if (hasNamespace != 0U) type->setOriginNamespace(std::move(originNamespace));

    std::uint32_t argumentCount = 0;
    if (!reader.u32(argumentCount)) return nullptr;
    for (std::uint32_t index = 0; index < argumentCount; ++index)
    {
        ast::TypeSyntax* argument = deserializeTypeSyntaxTree(arena, reader);
        if (argument == nullptr) return nullptr;
        if (tuple != 0U && index < tupleNames.size())
            type->addTupleElement(argument, tupleNames[index]);
        else
            type->addTypeArgument(argument);
    }
    std::uint8_t hasReturn = 0;
    std::uint8_t hasError = 0;
    if (!reader.byte(hasReturn) || !reader.byte(hasError)) return nullptr;
    if (hasReturn != 0U)
    {
        ast::TypeSyntax* returnType = deserializeTypeSyntaxTree(arena, reader);
        if (returnType == nullptr) return nullptr;
        type->setFunctionReturnType(returnType);
    }
    if (hasError != 0U)
    {
        ast::TypeSyntax* errorType = deserializeTypeSyntaxTree(arena, reader);
        if (errorType == nullptr) return nullptr;
        type->setFunctionErrorType(errorType);
    }
    return type;
}

// NSC305-C01. AstNode scalar/string fields (no pointers, no children, no type syntaxes): the node
// identity and flags. The child/typeSyntax graph is the next layer.
[[maybe_unused]] std::string serializeAstNodeScalars(const ast::AstNode& node)
{
    std::string out;
    appendU32(out, static_cast<std::uint32_t>(node.kind()));
    appendU32(out, node.range().begin().offset());
    appendU32(out, node.range().end().offset());
    appendString(out, node.text());
    appendString(out, node.auxiliaryText());
    appendString(out, node.argumentName());
    appendString(out, node.allocatorFailureProfile());
    appendU32(out, static_cast<std::uint32_t>(node.tokenKind()));
    appendU32(out, static_cast<std::uint32_t>(node.modifiers().size()));
    for (const lexer::TokenKind modifier : node.modifiers())
        appendU32(out, static_cast<std::uint32_t>(modifier));
    appendStringVector(out, node.genericParameters());
    appendString(out, node.lifetimeName());
    appendByte(out, node.flag() ? 1U : 0U);
    appendByte(out, node.isReference() ? 1U : 0U);
    appendByte(out, node.isVariadic() ? 1U : 0U);
    appendByte(out, node.isUnsafe() ? 1U : 0U);
    appendByte(out, static_cast<std::uint8_t>(node.floatingPointMode()));
    appendByte(out, static_cast<std::uint8_t>(node.ffiUnwindPolicy()));
    appendU64(out, node.integerValue());
    std::uint64_t floatingBits = 0;
    const double floatingValue = node.floatingValue();
    std::memcpy(&floatingBits, &floatingValue, sizeof(floatingBits));
    appendU64(out, floatingBits);
    return out;
}

// NSC305-C01. Inverse of serializeAstNodeScalars (allocates the node into `arena`).
[[maybe_unused]] ast::AstNode* deserializeAstNodeScalars(
    ast::AstArena& arena, ByteReader& reader)
{
    std::uint32_t kind = 0;
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
    if (!reader.u32(kind) || !reader.u32(begin) || !reader.u32(end)) return nullptr;
    ast::AstNode* node = arena.make<ast::AstNode>(
        static_cast<ast::AstNodeKind>(kind),
        SourceRange{SourceLocation{FileId{}, begin}, SourceLocation{FileId{}, end}});
    std::string text;
    std::string auxiliaryText;
    std::string argumentName;
    std::string allocatorFailureProfile;
    std::uint32_t tokenKind = 0;
    std::uint32_t modifierCount = 0;
    std::string lifetimeName;
    std::uint8_t flag = 0;
    std::uint8_t reference = 0;
    std::uint8_t variadic = 0;
    std::uint8_t unsafe = 0;
    std::uint8_t floatingPointMode = 0;
    std::uint8_t ffiUnwind = 0;
    std::uint64_t integerValue = 0;
    std::uint64_t floatingBits = 0;
    std::vector<std::string> genericParameters;
    if (!reader.string(text) || !reader.string(auxiliaryText) ||
        !reader.string(argumentName) || !reader.string(allocatorFailureProfile) ||
        !reader.u32(tokenKind) || !reader.u32(modifierCount))
        return nullptr;
    node->setText(std::move(text));
    node->setAuxiliaryText(std::move(auxiliaryText));
    node->setArgumentName(std::move(argumentName));
    node->setAllocatorFailureProfile(std::move(allocatorFailureProfile));
    node->setTokenKind(static_cast<lexer::TokenKind>(tokenKind));
    for (std::uint32_t index = 0; index < modifierCount; ++index)
    {
        std::uint32_t modifier = 0;
        if (!reader.u32(modifier)) return nullptr;
        node->addModifier(static_cast<lexer::TokenKind>(modifier));
    }
    if (!reader.stringVector(genericParameters) || !reader.string(lifetimeName) ||
        !reader.byte(flag) || !reader.byte(reference) || !reader.byte(variadic) ||
        !reader.byte(unsafe) || !reader.byte(floatingPointMode) || !reader.byte(ffiUnwind) ||
        !reader.u64(integerValue) || !reader.u64(floatingBits))
        return nullptr;
    for (const std::string& parameter : genericParameters)
        node->addGenericParameter(parameter);
    node->setLifetimeName(std::move(lifetimeName));
    node->setFlag(flag != 0U);
    node->setReference(reference != 0U);
    node->setVariadic(variadic != 0U);
    node->setUnsafe(unsafe != 0U);
    node->setFloatingPointMode(static_cast<ast::FloatingPointModeSyntaxKind>(floatingPointMode));
    node->setFfiUnwindPolicy(static_cast<ast::FfiUnwindSyntaxKind>(ffiUnwind));
    node->setIntegerValue(integerValue);
    double floatingValue = 0.0;
    std::memcpy(&floatingValue, &floatingBits, sizeof(floatingValue));
    node->setFloatingValue(floatingValue);
    return node;
}

// NSC305-C01. Recursive (tree) serialization of an AstNode: scalars + the typeSyntax/throwsType/
// typeSyntaxes graph + first/second/third + children. Nested generic parameter/constraint/lifetime
// structs are the next layer.
[[maybe_unused]] std::string serializeAstNodeTree(const ast::AstNode* node)
{
    if (node == nullptr) return {};
    std::string out = serializeAstNodeScalars(*node);
    const ast::TypeSyntax* typeSyntax = node->typeSyntax();
    const ast::TypeSyntax* throwsType = node->throwsTypeSyntax();
    appendByte(out, typeSyntax != nullptr ? 1U : 0U);
    appendByte(out, throwsType != nullptr ? 1U : 0U);
    if (typeSyntax != nullptr) out += serializeTypeSyntaxTree(typeSyntax);
    if (throwsType != nullptr) out += serializeTypeSyntaxTree(throwsType);
    appendU32(out, static_cast<std::uint32_t>(node->typeSyntaxes().size()));
    for (const ast::TypeSyntax* syntax : node->typeSyntaxes())
        out += serializeTypeSyntaxTree(syntax);
    const ast::AstNode* first = node->first();
    const ast::AstNode* second = node->second();
    const ast::AstNode* third = node->third();
    appendByte(out, first != nullptr ? 1U : 0U);
    appendByte(out, second != nullptr ? 1U : 0U);
    appendByte(out, third != nullptr ? 1U : 0U);
    if (first != nullptr) out += serializeAstNodeTree(first);
    if (second != nullptr) out += serializeAstNodeTree(second);
    if (third != nullptr) out += serializeAstNodeTree(third);
    appendU32(out, static_cast<std::uint32_t>(node->children().size()));
    for (const ast::AstNode* child : node->children())
        out += serializeAstNodeTree(child);
    return out;
}

// NSC305-C01. Inverse of serializeAstNodeTree (allocates the tree into `arena`).
[[maybe_unused]] ast::AstNode* deserializeAstNodeTree(
    ast::AstArena& arena, ByteReader& reader)
{
    ast::AstNode* node = deserializeAstNodeScalars(arena, reader);
    if (node == nullptr) return nullptr;
    std::uint8_t hasTypeSyntax = 0;
    std::uint8_t hasThrows = 0;
    if (!reader.byte(hasTypeSyntax) || !reader.byte(hasThrows)) return nullptr;
    if (hasTypeSyntax != 0U)
    {
        ast::TypeSyntax* type = deserializeTypeSyntaxTree(arena, reader);
        if (type == nullptr) return nullptr;
        node->setTypeSyntax(type);
    }
    if (hasThrows != 0U)
    {
        ast::TypeSyntax* type = deserializeTypeSyntaxTree(arena, reader);
        if (type == nullptr) return nullptr;
        node->setThrowsTypeSyntax(type);
    }
    std::uint32_t typeSyntaxCount = 0;
    if (!reader.u32(typeSyntaxCount)) return nullptr;
    for (std::uint32_t index = 0; index < typeSyntaxCount; ++index)
    {
        ast::TypeSyntax* type = deserializeTypeSyntaxTree(arena, reader);
        if (type == nullptr) return nullptr;
        node->addTypeSyntax(type);
    }
    std::uint8_t hasFirst = 0;
    std::uint8_t hasSecond = 0;
    std::uint8_t hasThird = 0;
    if (!reader.byte(hasFirst) || !reader.byte(hasSecond) || !reader.byte(hasThird)) return nullptr;
    if (hasFirst != 0U)
    {
        ast::AstNode* child = deserializeAstNodeTree(arena, reader);
        if (child == nullptr) return nullptr;
        node->setFirst(child);
    }
    if (hasSecond != 0U)
    {
        ast::AstNode* child = deserializeAstNodeTree(arena, reader);
        if (child == nullptr) return nullptr;
        node->setSecond(child);
    }
    if (hasThird != 0U)
    {
        ast::AstNode* child = deserializeAstNodeTree(arena, reader);
        if (child == nullptr) return nullptr;
        node->setThird(child);
    }
    std::uint32_t childCount = 0;
    if (!reader.u32(childCount)) return nullptr;
    for (std::uint32_t index = 0; index < childCount; ++index)
    {
        ast::AstNode* child = deserializeAstNodeTree(arena, reader);
        if (child == nullptr) return nullptr;
        node->addChild(child);
    }
    return node;
}

// NSC305-C01. GenericParameterSyntax + LifetimeParameterSyntax + LifetimeOutlivesConstraint (no
// TypeSyntax recursion in the parameter; constantType is serialized as a full tree).
[[maybe_unused]] std::string serializeGenericParameter(const ast::GenericParameterSyntax& parameter)
{
    std::string out;
    appendString(out, parameter.name);
    appendByte(out, static_cast<std::uint8_t>(parameter.kind));
    appendByte(out, parameter.constantType != nullptr ? 1U : 0U);
    if (parameter.constantType != nullptr)
        out += serializeTypeSyntaxTree(parameter.constantType);
    appendU32(out, parameter.range.begin().offset());
    appendU32(out, parameter.range.end().offset());
    return out;
}

// NSC305-C01. GenericConstraintClause (the source `where` clause; TypeSyntax references as trees).
[[maybe_unused]] std::string serializeGenericConstraint(const ast::GenericConstraintClause& clause)
{
    std::string out;
    appendString(out, clause.parameterName);
    appendU32(out, clause.parameterRange.begin().offset());
    appendU32(out, clause.parameterRange.end().offset());
    appendByte(out, clause.requiresClass ? 1U : 0U);
    appendByte(out, clause.requiresValueType ? 1U : 0U);
    appendByte(out, clause.requiresDefaultConstructor ? 1U : 0U);
    appendByte(out, clause.requiresCopy ? 1U : 0U);
    appendByte(out, clause.requiresMove ? 1U : 0U);
    appendByte(out, clause.requiresTrackedLifetime ? 1U : 0U);
    appendByte(out, clause.requiresUnmanaged ? 1U : 0U);
    appendByte(out, clause.requiresNotNull ? 1U : 0U);
    appendByte(out, clause.requiresRefLike ? 1U : 0U);
    appendByte(out, clause.equalityConstraint != nullptr ? 1U : 0U);
    if (clause.equalityConstraint != nullptr)
        out += serializeTypeSyntaxTree(clause.equalityConstraint);
    appendByte(out, clause.associatedProjection != nullptr ? 1U : 0U);
    if (clause.associatedProjection != nullptr)
        out += serializeTypeSyntaxTree(clause.associatedProjection);
    appendU32(out, static_cast<std::uint32_t>(clause.typeConstraints.size()));
    for (const ast::TypeSyntax* constraint : clause.typeConstraints)
        out += serializeTypeSyntaxTree(constraint);
    appendU32(out, clause.range.begin().offset());
    appendU32(out, clause.range.end().offset());
    appendByte(out, clause.substitutedArgument != nullptr ? 1U : 0U);
    if (clause.substitutedArgument != nullptr)
        out += serializeTypeSyntaxTree(clause.substitutedArgument);
    return out;
}

// NSC305-C01. Inverse of serializeGenericParameter (allocates the constantType into `arena`).
[[maybe_unused]] bool deserializeGenericParameter(
    ast::AstArena& arena, ByteReader& reader, ast::GenericParameterSyntax& parameter)
{
    std::uint8_t kind = 0;
    std::uint8_t hasConstantType = 0;
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
    if (!reader.string(parameter.name) || !reader.byte(kind) || !reader.byte(hasConstantType))
        return false;
    parameter.kind = static_cast<ast::GenericParameterSyntaxKind>(kind);
    if (hasConstantType != 0U)
    {
        parameter.constantType = deserializeTypeSyntaxTree(arena, reader);
        if (parameter.constantType == nullptr) return false;
    }
    if (!reader.u32(begin) || !reader.u32(end)) return false;
    parameter.range =
        SourceRange{SourceLocation{FileId{}, begin}, SourceLocation{FileId{}, end}};
    return true;
}

// NSC305-C01. Inverse of serializeGenericConstraint (allocates TypeSyntax references into `arena`).
[[maybe_unused]] bool deserializeGenericConstraint(
    ast::AstArena& arena, ByteReader& reader, ast::GenericConstraintClause& clause)
{
    std::uint8_t requiresClass = 0;
    std::uint8_t requiresValueType = 0;
    std::uint8_t requiresDefaultConstructor = 0;
    std::uint8_t requiresCopy = 0;
    std::uint8_t requiresMove = 0;
    std::uint8_t requiresTrackedLifetime = 0;
    std::uint8_t requiresUnmanaged = 0;
    std::uint8_t requiresNotNull = 0;
    std::uint8_t requiresRefLike = 0;
    std::uint8_t hasEquality = 0;
    std::uint8_t hasProjection = 0;
    std::uint8_t hasSubstituted = 0;
    std::uint32_t parameterBegin = 0;
    std::uint32_t parameterEnd = 0;
    std::uint32_t clauseBegin = 0;
    std::uint32_t clauseEnd = 0;
    std::uint32_t constraintCount = 0;
    if (!reader.string(clause.parameterName) || !reader.u32(parameterBegin) ||
        !reader.u32(parameterEnd) || !reader.byte(requiresClass) ||
        !reader.byte(requiresValueType) || !reader.byte(requiresDefaultConstructor) ||
        !reader.byte(requiresCopy) || !reader.byte(requiresMove) ||
        !reader.byte(requiresTrackedLifetime) || !reader.byte(requiresUnmanaged) ||
        !reader.byte(requiresNotNull) || !reader.byte(requiresRefLike) ||
        !reader.byte(hasEquality) || !reader.byte(hasProjection) || !reader.u32(constraintCount))
        return false;
    clause.parameterRange =
        SourceRange{SourceLocation{FileId{}, parameterBegin}, SourceLocation{FileId{}, parameterEnd}};
    clause.requiresClass = requiresClass != 0U;
    clause.requiresValueType = requiresValueType != 0U;
    clause.requiresDefaultConstructor = requiresDefaultConstructor != 0U;
    clause.requiresCopy = requiresCopy != 0U;
    clause.requiresMove = requiresMove != 0U;
    clause.requiresTrackedLifetime = requiresTrackedLifetime != 0U;
    clause.requiresUnmanaged = requiresUnmanaged != 0U;
    clause.requiresNotNull = requiresNotNull != 0U;
    clause.requiresRefLike = requiresRefLike != 0U;
    if (hasEquality != 0U)
    {
        clause.equalityConstraint = deserializeTypeSyntaxTree(arena, reader);
        if (clause.equalityConstraint == nullptr) return false;
    }
    if (hasProjection != 0U)
    {
        clause.associatedProjection = deserializeTypeSyntaxTree(arena, reader);
        if (clause.associatedProjection == nullptr) return false;
    }
    clause.typeConstraints.clear();
    for (std::uint32_t index = 0; index < constraintCount; ++index)
    {
        ast::TypeSyntax* constraint = deserializeTypeSyntaxTree(arena, reader);
        if (constraint == nullptr) return false;
        clause.typeConstraints.push_back(constraint);
    }
    if (!reader.u32(clauseBegin) || !reader.u32(clauseEnd) || !reader.byte(hasSubstituted))
        return false;
    clause.range = SourceRange{SourceLocation{FileId{}, clauseBegin}, SourceLocation{FileId{}, clauseEnd}};
    if (hasSubstituted != 0U)
    {
        clause.substitutedArgument = deserializeTypeSyntaxTree(arena, reader);
        if (clause.substitutedArgument == nullptr) return false;
    }
    return true;
}

// NSC305-C02. Full catalog entry: the record metadata followed by the AST subtree (the syntax node).
// The indexes are rebuilt from the restored records, so only these two are persisted.
[[maybe_unused]] std::string serializeCatalogEntry(const GenericTemplateRecord& record)
{
    std::string out = serializeTemplateRecord(record);
    out += serializeAstNodeTree(record.syntax);
    return out;
}

// NSC305-C02. Inverse of serializeCatalogEntry: reads the record metadata, then the syntax AST tree
// into `arena`, and links record.syntax to the restored node.
[[maybe_unused]] bool restoreCatalogEntry(
    ast::AstArena& arena, std::string_view bytes, GenericTemplateRecord& record)
{
    ByteReader reader(bytes);
    if (!readTemplateRecord(reader, record)) return false;
    record.syntax = deserializeAstNodeTree(arena, reader);
    return record.syntax != nullptr;
}
} // namespace

// NSC305-C01. Persists the collected template records to the C1 cache (the relocatable metadata
// payload). The AST subtree restore is a later layer; this stores the record skeleton.
void ProjectGenericModel::storeTemplateRecords()
{
    if (!templateCache_.enabled()) return;
    for (const TemplateState& state : templateStates_)
    {
        const std::string payload = serializeCatalogEntry(state.record);
        std::string error;
        (void)templateCache_.store(state.record.c1CacheKey, payload, error);
    }
}

// NSC305-C01/C02. Warm-build restore: look up the C1/C2 cache and count a hit when the FULL catalog
// entry (record + syntax AST) round-trips into the unit arena. The AST splice into the unit tree is
// a later layer; this validates the persisted snapshot.
void ProjectGenericModel::restoreTemplateRecords()
{
    if (!templateCache_.enabled()) return;
    for (const TemplateState& state : templateStates_)
    {
        std::string payload;
        bool hit = false;
        std::string error;
        if (templateCache_.restore(state.record.c1CacheKey, payload, hit, error) && hit)
        {
            GenericTemplateRecord restored;
            if (restoreCatalogEntry(state.unit->arena(), payload, restored))
            {
                telemetry_.noteSpecializationHit();
                ++instantiationCacheHitCount_;
            }
        }
    }
}

void ProjectGenericModel::collectTemplates(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    ast::AstNode* parent,
    std::string namespaceName,
    std::string ownerName,
    bool insideGenericType,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // NSC305-G05. Iterative pre-order traversal with an explicit stack. The per-node logic is
    // unchanged; only the recursion is replaced so deep syntax cannot exhaust the process stack.
    struct CollectFrame
    {
        ast::AstNode* node;
        ast::AstNode* parent;
        std::string namespaceName;
        std::string ownerName;
        bool insideGenericType;
    };
    std::vector<CollectFrame> stack;
    stack.push_back(CollectFrame{node, parent, std::move(namespaceName), std::move(ownerName),
        insideGenericType});
    while (!stack.empty())
    {
        CollectFrame frame = std::move(stack.back());
        stack.pop_back();
        node = frame.node;
        parent = frame.parent;
        namespaceName = std::move(frame.namespaceName);
        ownerName = std::move(frame.ownerName);
        insideGenericType = frame.insideGenericType;
        if (node == nullptr) continue;

    std::string currentNamespace = namespaceName;
    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
    {
        currentNamespace = namespaceAppend(namespaceName, node->text());
    }
    if (node->kind() == ast::AstNodeKind::ClassDeclaration &&
        node->auxiliaryText() == "ns3-trait-impl")
        continue;

    if (!insideGenericType && node->kind() == ast::AstNodeKind::EnumDeclaration)
    {
        const std::string qualified = qualifiedName(currentNamespace, node->text());
        const std::string enumTypeKey =
            "enum:" + std::string(unit.moduleName()) + "::" + qualified;
        const auto publishType = [&](const std::string& spelling)
        {
            const auto [entry, inserted] = constEnumTypeBySpelling_.emplace(spelling, enumTypeKey);
            if (!inserted && entry->second != enumTypeKey) entry->second.clear();
        };
        publishType(node->text());
        publishType(qualified);

        const std::string underlying = node->typeSyntax() != nullptr
            ? "builtin:" + node->typeSyntax()->baseName() : "builtin:int";
        EnumConstResolver enumResolver(node->text(), underlying);
        std::uint64_t nextValue = 0;
        bool previousKnown = true;
        for (const ast::AstNode* member : node->children())
        {
            if (member == nullptr || member->kind() != ast::AstNodeKind::EnumMemberDeclaration)
                continue;
            std::uint64_t bits = nextValue;
            if (member->first() != nullptr)
            {
                const ProjectIntegerEvaluator evaluator(&enumResolver);
                const std::optional<ProjectIntegerValue> evaluated = evaluator.evaluate(member->first());
                previousKnown = evaluated.has_value();
                if (previousKnown) bits = evaluated->bits;
            }
            if (!previousKnown) continue;
            bits = ProjectIntegerEvaluator::normalize(bits, underlying);
            const std::string value = ProjectIntegerEvaluator::isSignedInteger(underlying) &&
                    static_cast<std::int64_t>(bits) < 0
                ? std::to_string(static_cast<std::int64_t>(bits))
                : std::to_string(bits);
            const std::pair<std::string, std::string> identity{enumTypeKey, value};
            const auto publishValue = [&](const std::string& spelling)
            {
                const auto [entry, inserted] =
                    constEnumValueBySpelling_.emplace(spelling, identity);
                if (!inserted && entry->second != identity) entry->second = {};
            };
            publishValue(node->text() + "." + member->text());
            publishValue(qualified + "." + member->text());
            enumResolver.add(member->text(), bits);
            nextValue = bits + 1U;
        }
    }

    // NSC-LIMIT-022. Generic specialization happens before ProjectNameLookup exists, but its
    // cache key still has to identify the DECLARATION of an argument, not the project in which
    // that argument happened to be written. Record ordinary named types now, from the same
    // already-parsed graph. Generic declarations are represented by TemplateState and nested
    // declarations inside a generic type are not predeclared by the later name model either.
    if (!insideGenericType && isNamedTypeDeclaration(node->kind()) &&
        node->genericParameters().empty())
    {
        const std::string qualifiedTypeName = appendOwner(ownerName, node->text());
        std::string sourceName = qualifiedName(currentNamespace, qualifiedTypeName);
        std::replace(sourceName.begin(), sourceName.end(), '+', '.');
        declaredTypeIdentities_.push_back(DeclaredTypeIdentity{
            unit.projectOrdinal(),
            currentNamespace,
            std::move(sourceName),
            std::string(unit.moduleName()) + "::" +
                qualifiedName(currentNamespace, qualifiedTypeName),
            node->tokenKind() == lexer::TokenKind::KwPublic});
    }

    const bool genericType = isGenericTypeTemplateNode(node);
    if (genericType)
    {
        if (node->flag())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                node->range(),
                "partial generic types are deferred beyond Stage 44; a monomorphized template must have one canonical body");
        }
        GenericTemplateRecord record;
        record.kind = genericTypeTemplateKind(node);
        record.accessibility = genericTemplateAccessibility(node, false);
        record.syntax = node;
        record.projectOrdinal = unit.projectOrdinal();
        record.sourceOrdinal = unit.sourceOrdinal();
        record.moduleName = std::string(unit.moduleName());
        record.namespaceName = currentNamespace;
        record.name = node->text();
        record.ownerName = ownerName;
        record.parameters = node->genericParameters();
        record.parameterSyntaxes = node->genericParameterSyntaxes();
        record.constraints = node->genericConstraints();
        if (const auto extensions = traitImplExtensions_.find(node);
            extensions != traitImplExtensions_.end())
            record.extensionSyntaxes = extensions->second;
        // NSC-LIMIT-009: `ns.A+Inner`1`, no `ns.Inner`1`. Hoy el parser rechaza un tipo anidado
        // generico, de modo que esta cadena esta siempre vacia; se escribe igualmente porque el
        // dia que deje de rechazarlo la clave tiene que salir completa, no ambigua.
        record.qualifiedTypeName = appendOwner(ownerName, record.name);
        record.canonicalKey = record.moduleName + "::" +
            qualifiedName(currentNamespace, record.qualifiedTypeName) +
            "`" + genericAritySpelling(record);
        record.sourceFingerprint = computeTemplateSourceFingerprint(unit, node);
        record.c1CacheKey = templateCacheKey(record.sourceFingerprint);
        telemetry_.noteTemplate(static_cast<std::size_t>(record.kind));
        templateStates_.push_back(TemplateState{record, &unit, node, parent, templateStates_.size()});
    }

    if (isGenericMethodTemplateNode(node))
    {
        if (insideGenericType)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::ExpectedDeclaration,
                diagnostics::DiagnosticSeverity::Error,
                node->range(),
                "generic methods declared inside generic types are deliberately deferred beyond Stage 44; specialize one generic dimension at a time");
        }
        GenericTemplateRecord record;
        record.kind = GenericTemplateKind::Method;
        record.accessibility = genericTemplateAccessibility(
            node, node->kind() == ast::AstNodeKind::MethodDeclaration);
        record.syntax = node;
        record.projectOrdinal = unit.projectOrdinal();
        record.sourceOrdinal = unit.sourceOrdinal();
        record.moduleName = std::string(unit.moduleName());
        record.namespaceName = currentNamespace;
        record.ownerName = ownerName;
        record.name = node->text();
        record.parameters = node->genericParameters();
        record.parameterSyntaxes = node->genericParameterSyntaxes();
        record.constraints = node->genericConstraints();
        record.qualifiedTypeName = ownerName;
        const std::string callableScope = ownerName.empty()
            ? currentNamespace
            : qualifiedName(currentNamespace, ownerName);
        record.canonicalKey = record.moduleName + "::" +
            (callableScope.empty() ? std::string{} : callableScope + ".") +
            record.name + "``" + genericAritySpelling(record);
        record.sourceFingerprint = computeTemplateSourceFingerprint(unit, node);
        record.c1CacheKey = templateCacheKey(record.sourceFingerprint);
        record.specializationFamilyKey = methodSpecializationFamilyKey(record, node);
        record.isControlledSpecialization =
            node->auxiliaryText() == "controlled-specialization";
        telemetry_.noteTemplate(static_cast<std::size_t>(record.kind));
        templateStates_.push_back(TemplateState{record, &unit, node, parent, templateStates_.size()});
    }

    // NSC-LIMIT-009: se APILA. La version anterior reemplazaba, de modo que al entrar en
    // `A { Inner { ... } }` el dueno pasaba a ser «Inner» y `A` desaparecia.
    const std::string childOwner =
        (node->kind() == ast::AstNodeKind::ClassDeclaration || node->kind() == ast::AstNodeKind::StructDeclaration)
            ? appendOwner(ownerName, node->text()) : ownerName;
    const bool childInsideGeneric = insideGenericType || genericType;
    const std::vector<ast::AstNode*> children(node->children().begin(), node->children().end());
    for (auto iterator = children.rbegin(); iterator != children.rend(); ++iterator)
    {
        stack.push_back(CollectFrame{*iterator, node, currentNamespace, childOwner,
            childInsideGeneric});
    }
    }
}

bool ProjectGenericModel::specializeGenericLambdaCall(
    ProjectSyntaxUnit& unit,
    ast::AstNode* call,
    const GenericLookupScope::GenericLambda& local,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (call == nullptr || local.templateSyntax == nullptr || local.declaration == nullptr)
        return false;
    const ast::AstNode* source = local.templateSyntax;
    const auto& parameters = source->genericParameterSyntaxes();
    if (parameters.empty()) return false;

    GenericTemplateRecord inferenceRecord;
    inferenceRecord.parameters = source->genericParameters();
    inferenceRecord.parameterSyntaxes = parameters;
    inferenceRecord.constraints = source->genericConstraints();
    const std::unordered_set<std::string> names(
        inferenceRecord.parameters.begin(), inferenceRecord.parameters.end());
    std::unordered_map<std::string, ast::TypeSyntax*> inferred;
    std::unordered_map<std::string, std::vector<const ast::TypeSyntax*>> packSubstitutions;

    if (!call->typeSyntaxes().empty() || call->flag())
    {
        const bool trailingPack = !parameters.empty() &&
            (parameters.back().kind == ast::GenericParameterSyntaxKind::TypePack ||
             parameters.back().kind == ast::GenericParameterSyntaxKind::ConstantPack);
        const std::size_t fixedCount = parameters.size() - (trailingPack ? 1U : 0U);
        if ((!trailingPack && call->typeSyntaxes().size() != fixedCount) ||
            (trailingPack && call->typeSyntaxes().size() < fixedCount))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArgumentCount,
                diagnostics::DiagnosticSeverity::Error,
                call->range(),
                "generic lambda expects " + std::to_string(fixedCount) +
                    (trailingPack ? " or more" : std::string{}) +
                    " generic arguments but received " +
                    std::to_string(call->typeSyntaxes().size()));
            return false;
        }
        for (std::size_t index = 0; index < fixedCount; ++index)
            inferred.emplace(parameters[index].name, call->typeSyntaxes()[index]);
        if (trailingPack)
        {
            std::vector<const ast::TypeSyntax*>& values =
                packSubstitutions[parameters.back().name];
            values.insert(
                values.end(), call->typeSyntaxes().begin() +
                    static_cast<std::ptrdiff_t>(fixedCount), call->typeSyntaxes().end());
        }
    }
    else
    {
        std::vector<const ast::AstNode*> valueParameters;
        for (const ast::AstNode* child : source->children())
            if (child != nullptr && child->kind() == ast::AstNodeKind::ParameterDeclaration)
                valueParameters.push_back(child);
        if (valueParameters.size() != call->children().size())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArgumentCount,
                diagnostics::DiagnosticSeverity::Error,
                call->range(), "generic lambda argument count does not match its parameter list");
            return false;
        }
        for (std::size_t index = 0; index < valueParameters.size(); ++index)
        {
            const ast::TypeSyntax* actual = inferExpressionType(unit, call->children()[index], scope);
            if (actual == nullptr || valueParameters[index]->typeSyntax() == nullptr ||
                !inferTypeArguments(
                    unit, valueParameters[index]->typeSyntax(), actual, inferenceRecord, names,
                    inferred))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidCall,
                    diagnostics::DiagnosticSeverity::Error,
                    call->children()[index]->range(),
                    "cannot infer generic-lambda arguments from value argument " +
                        std::to_string(index + 1U));
                return false;
            }
        }
    }

    std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
    for (const ast::GenericParameterSyntax& parameter : parameters)
    {
        if (parameter.kind == ast::GenericParameterSyntaxKind::TypePack ||
            parameter.kind == ast::GenericParameterSyntaxKind::ConstantPack)
        {
            if (!packSubstitutions.contains(parameter.name))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidCall,
                    diagnostics::DiagnosticSeverity::Error,
                    call->range(),
                    "generic-lambda pack '" + parameter.name +
                        "' requires explicit generic arguments");
                return false;
            }
            continue;
        }
        const auto found = inferred.find(parameter.name);
        if (found == inferred.end())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                call->range(),
                "generic-lambda parameter '" + parameter.name + "' could not be inferred");
            return false;
        }
        substitutions.emplace(parameter.name, found->second);
    }

    const auto savedPackSubstitutions = activePackSubstitutions_;
    activePackSubstitutions_ = packSubstitutions;
    ast::AstNode* specialized = cloneNode(unit.arena(), source, substitutions, {}, {});
    activePackSubstitutions_ = savedPackSubstitutions;
    specialized->clearGenericParameters();
    const std::string localName = call->first() != nullptr ? call->first()->text() : std::string{};
    specialized->setText("generic-lambda-instance:" + localName);
    const auto markRecursiveCalls = [&](auto&& self, ast::AstNode* node) -> void
    {
        if (node == nullptr) return;
        if (node->kind() == ast::AstNodeKind::CallExpression && node->first() != nullptr &&
            node->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
            node->first()->text() == localName)
        {
            node->first()->setAuxiliaryText("generic-lambda-self");
        }
        self(self, node->first());
        self(self, node->second());
        self(self, node->third());
        for (ast::AstNode* child : node->children()) self(self, child);
    };
    markRecursiveCalls(markRecursiveCalls, specialized->first());
    if (!specialized->flag() && specialized->first() != nullptr)
    {
        GenericLookupScope bodyScope = scope;
        for (const ast::AstNode* parameter : specialized->children())
        {
            if (parameter != nullptr &&
                parameter->kind() == ast::AstNodeKind::ParameterDeclaration &&
                parameter->typeSyntax() != nullptr)
            {
                bodyScope.addValueType(parameter->text(), parameter->typeSyntax());
            }
        }
        if (ast::TypeSyntax* inferredReturn =
                inferExpressionType(unit, specialized->first(), bodyScope))
            specialized->setTypeSyntax(inferredReturn);
        else
        {
            // A captured name can be intentionally absent from the early generic value-type
            // table (the definitive body model owns capture lookup). Use the first concrete
            // value parameter as a bounded return candidate; semantic analysis still verifies
            // the complete body conversion against it before HIR is built.
            const auto firstParameter = std::ranges::find_if(
                specialized->children(), [](const ast::AstNode* item)
                {
                    return item != nullptr &&
                        item->kind() == ast::AstNodeKind::ParameterDeclaration &&
                        item->typeSyntax() != nullptr;
                });
            if (firstParameter != specialized->children().end())
            {
                bool conflict = false;
                specialized->setTypeSyntax(cloneAliasType(
                    unit.arena(), (*firstParameter)->typeSyntax(), {}, conflict));
            }
        }
    }

    std::size_t templateOrdinal = 0;
    const auto priorTemplate = genericLambdaTemplateOrdinals_.find(source);
    if (priorTemplate == genericLambdaTemplateOrdinals_.end())
    {
        GenericTemplateRecord record;
        record.kind = GenericTemplateKind::Method;
        record.syntax = source;
        record.projectOrdinal = unit.projectOrdinal();
        record.moduleName = unit.projectOrdinal() < projectModuleNames_.size()
            ? projectModuleNames_[unit.projectOrdinal()] : std::string{};
        record.name = "lambda@" + std::to_string(source->range().begin().offset());
        record.parameters = source->genericParameters();
        record.parameterSyntaxes = source->genericParameterSyntaxes();
        record.constraints = source->genericConstraints();
        record.canonicalKey = record.moduleName + "::<local>::" + record.name;
        templateOrdinal = templateRecords_.size();
        templateRecords_.push_back(std::move(record));
        genericLambdaTemplateOrdinals_.emplace(source, templateOrdinal);
    }
    else
    {
        templateOrdinal = priorTemplate->second;
    }
    GenericSpecializationRecord specialization;
    specialization.kind = GenericTemplateKind::Method;
    specialization.templateSyntax = source;
    specialization.specializedSyntax = specialized;
    specialization.templateOrdinal = templateOrdinal;
    specialization.instantiationSyntax = call;
    specialization.instantiationRange = call->range();
    specialization.cacheOrdinal = specializations_.size();
    specialization.templateKey = templateRecords_[templateOrdinal].canonicalKey;
    specialization.specializedName = "lambda-instance-" +
        std::to_string(specialization.cacheOrdinal);
    for (const ast::GenericParameterSyntax& parameter : parameters)
    {
        if (const auto pack = packSubstitutions.find(parameter.name);
            pack != packSubstitutions.end())
        {
            for (const ast::TypeSyntax* argument : pack->second)
            {
                const std::string identity = typeSpelling(argument);
                specialization.typeArguments.push_back(identity);
                specialization.specializationKey += "|" + identity;
            }
        }
        else
        {
            const ast::TypeSyntax* argument = substitutions.at(parameter.name);
            const std::string identity = typeSpelling(argument);
            specialization.typeArguments.push_back(identity);
            specialization.specializationKey += "|" + identity;
        }
    }
    const std::string instanceKey = specialization.specializationKey;
    const std::size_t instanceOrdinal = specialization.cacheOrdinal;
    specializations_.push_back(std::move(specialization));
    specializations_.back().state = SpecializationState::Ready;
    const bool captures = source->auxiliaryText().starts_with("captures:") &&
        source->auxiliaryText().size() > std::string_view("captures:").size();
    const auto synthesizeDelegate = [&](ast::AstNode* lambda, std::string_view suffix)
        -> std::optional<std::string>
    {
        ast::AstNode* declarationContainer = local.declarationContainer != nullptr
            ? local.declarationContainer : unit.syntaxTree();
        if (declarationContainer == nullptr || lambda == nullptr || lambda->typeSyntax() == nullptr)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                call->range(),
                "capturing generic lambda cannot synthesize its concrete delegate carrier (" +
                    std::string(declarationContainer == nullptr ? "missing container" : "missing return type") + ")");
            return std::nullopt;
        }
        const std::string delegateName = "__nsc_generic_closure_" +
            std::to_string(unit.sourceOrdinal()) + "_" +
            std::to_string(source->range().begin().offset()) + std::string(suffix);
        ast::AstNode* delegate = unit.arena().make<ast::AstNode>(
            ast::AstNodeKind::DelegateDeclaration, source->range());
        delegate->setText(delegateName);
        bool ownershipConflict = false;
        delegate->setTypeSyntax(cloneAliasType(
            unit.arena(), lambda->typeSyntax(), {}, ownershipConflict));
        for (const ast::AstNode* parameter : lambda->children())
        {
            if (parameter != nullptr &&
                parameter->kind() == ast::AstNodeKind::ParameterDeclaration)
                delegate->addChild(cloneNode(unit.arena(), parameter, {}, {}, {}));
        }
        declarationContainer->addChild(delegate);
        return delegateName;
    };
    if (!captures)
    {
        call->setFirst(specialized);
    }
    else if (const auto carrier = genericLambdaCarrierKeys_.find(source);
             carrier != genericLambdaCarrierKeys_.end() && carrier->second != instanceKey)
    {
        const std::optional<std::string> delegateName = synthesizeDelegate(
            specialized, "_" + std::to_string(instanceOrdinal));
        if (!delegateName.has_value()) return false;
        call->setTypeSyntax(unit.arena().make<ast::TypeSyntax>(
            source->range(), lexer::TokenKind::Identifier, *delegateName));
        call->setSecond(specialized);
        call->setAuxiliaryText("generic-lambda-specialized-call");
    }
    call->clearTypeSyntaxes();
    call->setFlag(false);

    // Keep the lexical binding a concrete zero-size lambda value as well. Calls are rewritten to
    // their own concrete callable, so this carrier is never used for erased dispatch; it merely
    // gives `var` an ABI-valid native type and preserves ordinary declaration/capture analysis.
    if (materializedGenericLambdas_.insert(source).second)
    {
        ast::AstNode* carrier = cloneNode(unit.arena(), specialized, {}, {}, {});
        carrier->setText("generic-lambda-carrier");
        local.declaration->setFirst(carrier);
        genericLambdaCarrierKeys_[source] = instanceKey;
        if (captures)
        {
            const std::optional<std::string> delegateName = synthesizeDelegate(carrier, "");
            if (!delegateName.has_value()) return false;
            local.declaration->setTypeSyntax(unit.arena().make<ast::TypeSyntax>(
                source->range(), lexer::TokenKind::Identifier, *delegateName));
        }
    }
    return true;
}

// NSC305-G08. Builds the lexical scope for one node (see the header).
void ProjectGenericModel::prepareNodeScope(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    const std::string& namespaceName,
    const GenericLookupScope& scope,
    std::string& outNamespace,
    GenericLookupScope& outScope)
{
    outNamespace = namespaceName;
    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
    {
        outNamespace = namespaceAppend(namespaceName, node->text());
    }
    telemetry_.noteScopeCopy(
        scope.importedNamespaces.read().size() + scope.aliases.read().size() +
        scope.typeAliases.read().size() + scope.typeParameters.read().size() +
        scope.valuePassKinds.read().size() + scope.genericLambdas.read().size());
    outScope = scope;
    if (node->kind() == ast::AstNodeKind::LambdaExpression &&
        node->text().starts_with("generic-lambda-instance:"))
    {
        outScope.genericLambdas.write().erase(
            node->text().substr(std::string_view("generic-lambda-instance:").size()));
    }
    for (const ast::AstNode* child : node->children())
    {
        if (child == nullptr) continue;
        if (child->kind() == ast::AstNodeKind::UsingNamespaceDirective)
        {
            const std::string written = child->text();
            if (written.empty()) continue;
            const auto add = [&](std::string candidate)
            {
                auto& imported = outScope.importedNamespaces.write();
                if (std::find(imported.begin(), imported.end(), candidate) == imported.end())
                    imported.push_back(std::move(candidate));
            };
            const std::vector<std::string> segments = splitDotted(outNamespace);
            for (std::size_t depth = segments.size(); depth > 0; --depth)
            {
                std::string prefixed;
                for (std::size_t index = 0; index < depth; ++index)
                {
                    if (!prefixed.empty()) prefixed.push_back('.');
                    prefixed += segments[index];
                }
                prefixed.push_back('.');
                prefixed += written;
                add(std::move(prefixed));
            }
            add(written);
        }
        else if (child->kind() == ast::AstNodeKind::UsingAliasDirective)
        {
            if (!child->auxiliaryText().empty())
            {
                outScope.aliases.write()[child->text()] = child->auxiliaryText();
            }
        }
    }
    if ((node->kind() == ast::AstNodeKind::MethodDeclaration ||
         node->kind() == ast::AstNodeKind::FunctionDeclaration) &&
        node->typeSyntax() != nullptr)
    {
        bool conflict = false;
        outScope.expectedReturnType =
            cloneAliasType(unit.arena(), node->typeSyntax(), {}, conflict);
    }
    for (const ast::AstNode* child : node->children())
    {
        if (child == nullptr || child->typeSyntax() == nullptr ||
            (child->kind() != ast::AstNodeKind::ParameterDeclaration &&
             child->kind() != ast::AstNodeKind::FieldDeclaration))
            continue;
        bool conflict = false;
        outScope.addValueType(child->text(),
            cloneAliasType(unit.arena(), child->typeSyntax(), {}, conflict));
        if (child->kind() == ast::AstNodeKind::ParameterDeclaration)
        {
            outScope.valuePassKinds.write().erase(child->text());
            if (child->auxiliaryText().starts_with("pass:"))
            {
                const std::size_t end = child->auxiliaryText().find('|');
                outScope.valuePassKinds.write()[child->text()] =
                    child->auxiliaryText().substr(5U, end == std::string::npos ? end : end - 5U);
            }
        }
    }
}

// NSC305-G08. The per-node body of processNode (see the header). Returns false when the node's
// children must be skipped (alias directive or open template).
bool ProjectGenericModel::processNodeOnce(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    const std::string& currentNamespace,
    const std::string& ownerName,
    GenericLookupScope& currentScope,
    bool insideTemplate,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // MatchExpression arm conditions and bindings use MatchValueExpression to read the one-time
    // governing value. Preserve its structural source type lexically so body-only demands can
    // materialize closed view types before Sema. Match statements expose an ordinary hidden local.
    if (node->kind() == ast::AstNodeKind::MatchExpression && node->first() != nullptr)
    {
        currentScope.matchValuePassKind.clear();
        if (ast::TypeSyntax* governingType = inferExpressionType(unit, node->first(), currentScope))
            currentScope.matchValueType = governingType;
        if (node->first()->kind() == ast::AstNodeKind::IdentifierExpression)
        {
            const auto pass = currentScope.valuePassKinds.read().find(node->first()->text());
            if (pass != currentScope.valuePassKinds.read().end())
                currentScope.matchValuePassKind = pass->second;
        }
    }

    // The declaration is a compile-time rewrite rule, not a use of its still-open target.
    if (node->kind() == ast::AstNodeKind::UsingAliasDirective) return false;

    const bool genericLambdaTemplate =
        node->kind() == ast::AstNodeKind::LambdaExpression &&
        !node->genericParameters().empty();
    const bool templateNode = isGenericTypeTemplateNode(node) ||
        isGenericMethodTemplateNode(node) || genericLambdaTemplate;
    for (const ast::GenericConstraintClause& clause : node->genericConstraints())
    {
        for (const ast::TypeSyntax* constraint : clause.typeConstraints)
            processTypeSyntax(
                unit, const_cast<ast::TypeSyntax*>(constraint), currentNamespace, currentScope,
                diagnostics);
        std::string substitutedConstantType;
        std::string substitutedConstantValue;
        if (clause.substitutedArgument != nullptr &&
            !language::parseConstGenericArgumentMarker(
                clause.substitutedArgument->baseName(),
                substitutedConstantType,
                substitutedConstantValue))
        {
            processTypeSyntax(
                unit, const_cast<ast::TypeSyntax*>(clause.substitutedArgument), currentNamespace,
                currentScope, diagnostics);
        }
    }
    if (templateNode || insideTemplate)
    {
        if (isGenericMethodTemplateNode(node))
        {
            GenericLookupScope receiverScope = currentScope;
            for (const std::string& parameter : node->genericParameters())
                receiverScope.typeParameters.write().insert(parameter);
            for (const ast::AstNode* child : node->children())
            {
                if (child == nullptr ||
                    child->kind() != ast::AstNodeKind::ParameterDeclaration ||
                    child->typeSyntax() == nullptr)
                {
                    continue;
                }
                processTypeSyntax(
                    unit, const_cast<ast::TypeSyntax*>(child->typeSyntax()), currentNamespace,
                    receiverScope, diagnostics);
                break;
            }
        }
        return false;
    }

    ast::TypeSyntax* declarationExpectedType = nullptr;
    if (node->kind() == ast::AstNodeKind::VariableDeclarationStatement &&
        node->typeSyntax() != nullptr &&
        node->typeSyntax()->baseTokenKind() != lexer::TokenKind::KwVar)
    {
        bool conflict = false;
        declarationExpectedType =
            cloneAliasType(unit.arena(), node->typeSyntax(), {}, conflict);
    }

    if (node->kind() == ast::AstNodeKind::NameOfExpression &&
        node->typeSyntax() != nullptr && !node->typeSyntax()->typeArguments().empty())
    {
        ast::TypeSyntax* candidate = node->typeSyntax();
        TypeResolution typeResolution = resolveTypeTemplate(
            unit, currentNamespace, candidate->baseName(), candidate->typeArguments().size());
        if (typeResolution.kind != TypeResolutionKind::Unique)
        {
            typeResolution = resolveTypeTemplate(
                unit, currentNamespace, candidate->baseName(),
                std::numeric_limits<std::size_t>::max());
        }
        if (typeResolution.kind == TypeResolutionKind::Unique)
        {
            processTypeSyntax(unit, candidate, currentNamespace, currentScope, diagnostics);
        }
        else
        {
            for (ast::TypeSyntax* argument : candidate->typeArguments())
                processTypeSyntax(unit, argument, currentNamespace, currentScope, diagnostics);
        }
    }
    else if (node->kind() == ast::AstNodeKind::ExtensionDeclaration && node->typeSyntax() != nullptr)
    {
        GenericLookupScope extensionScope = currentScope;
        for (const std::string& parameter : node->genericParameters())
            extensionScope.typeParameters.write().insert(parameter);
        processTypeSyntax(unit, node->typeSyntax(), currentNamespace, extensionScope, diagnostics);
    }
    else if (node->typeSyntax() != nullptr)
    {
        processTypeSyntax(unit, node->typeSyntax(), currentNamespace, currentScope, diagnostics);
    }

    collectBodyTypeSpecializationDemand(unit, node, currentNamespace, currentScope);

    bool specializedGenericLambda = false;
    if (node->kind() == ast::AstNodeKind::CallExpression && node->first() != nullptr &&
        node->first()->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        const auto local = currentScope.genericLambdas.read().find(node->first()->text());
        if (local != currentScope.genericLambdas.read().end())
            specializedGenericLambda = specializeGenericLambdaCall(
                unit, node, local->second, currentScope, diagnostics);
    }
    if (!specializedGenericLambda && node->kind() == ast::AstNodeKind::CallExpression &&
        node->typeSyntaxes().empty() && !node->flag())
    {
        inferGenericCall(
            unit, node, currentNamespace, ownerName, currentScope, diagnostics);
    }
    if (!specializedGenericLambda && node->kind() == ast::AstNodeKind::CallExpression &&
        (!node->typeSyntaxes().empty() || node->flag()))
    {
        processGenericCall(unit, node, currentNamespace, ownerName, currentScope, diagnostics);
    }
    else
    {
        for (ast::TypeSyntax* syntax : node->typeSyntaxes())
        {
            processTypeSyntax(unit, syntax, currentNamespace, currentScope, diagnostics);
        }
    }

    // Bidirectional inference is deliberately bounded to contexts with one exact expected type.
    ast::AstNode* contextualCall = nullptr;
    const ast::TypeSyntax* contextualType = nullptr;
    if (node->kind() == ast::AstNodeKind::VariableDeclarationStatement)
    {
        contextualCall = node->first();
        contextualType = declarationExpectedType;
    }
    else if (node->kind() == ast::AstNodeKind::ReturnStatement)
    {
        contextualCall = node->first();
        contextualType = currentScope.expectedReturnType;
    }
    else if (node->kind() == ast::AstNodeKind::AssignmentExpression)
    {
        contextualCall = node->second();
        contextualType = inferExpressionType(unit, node->first(), currentScope);
    }
    if (contextualCall != nullptr && contextualType != nullptr)
        applyExpectedTypeToExpression(
            unit, contextualCall, contextualType, currentNamespace, ownerName, currentScope,
            diagnostics);
    return true;
}

void ProjectGenericModel::processNode(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    std::string namespaceName,
    std::string ownerName,
    GenericLookupScope scope,
    bool insideTemplate,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // NSC305-G08. Iterative pre-order traversal with an explicit stack; the per-node logic is
    // unchanged, only the recursion into first/second/third/children is removed. The sequential
    // local accumulation is preserved by computing each child's scope snapshot before the walk.
    struct Frame
    {
        ast::AstNode* node;
        std::string namespaceName;
        std::string ownerName;
        GenericLookupScope scope;
        bool insideTemplate;
    };
    std::vector<Frame> frames;
    frames.push_back(
        Frame{node, std::move(namespaceName), std::move(ownerName), std::move(scope), insideTemplate});
    while (!frames.empty())
    {
        Frame frame = std::move(frames.back());
        frames.pop_back();
        node = frame.node;
        namespaceName = std::move(frame.namespaceName);
        ownerName = std::move(frame.ownerName);
        scope = std::move(frame.scope);
        insideTemplate = frame.insideTemplate;
        if (node == nullptr) continue;

    std::string currentNamespace;
    GenericLookupScope currentScope;
    prepareNodeScope(unit, node, namespaceName, scope, currentNamespace, currentScope);

    if (!processNodeOnce(
            unit, node, currentNamespace, ownerName, currentScope, insideTemplate, diagnostics))
        continue;

    // NSC-LIMIT-009: se APILA. La version anterior reemplazaba, de modo que al entrar en
    // `A { Inner { ... } }` el dueno pasaba a ser «Inner» y `A` desaparecia.
    const std::string childOwner =
        (node->kind() == ast::AstNodeKind::ClassDeclaration || node->kind() == ast::AstNodeKind::StructDeclaration)
            ? appendOwner(ownerName, node->text()) : ownerName;
    const std::vector<ast::AstNode*> children(node->children().begin(), node->children().end());
    // NSC305-G08. Pre-compute each child's scope snapshot (the sequential local accumulation),
    // then push children in reverse so the stack pops them in source order.
    std::vector<GenericLookupScope> childScopes;
    childScopes.reserve(children.size());
    GenericLookupScope sequentialScope = currentScope;
    for (ast::AstNode* child : children)
    {
        ast::TypeSyntax* declaredLocalType = nullptr;
        if (child != nullptr && child->kind() == ast::AstNodeKind::VariableDeclarationStatement &&
            child->typeSyntax() != nullptr &&
            child->typeSyntax()->baseTokenKind() != lexer::TokenKind::KwVar)
        {
            bool conflict = false;
            declaredLocalType =
                cloneAliasType(unit.arena(), child->typeSyntax(), {}, conflict);
        }
        else if (child != nullptr &&
                 child->kind() == ast::AstNodeKind::VariableDeclarationStatement &&
                 child->first() != nullptr)
        {
            // Capture `var` from the initializer before its closed generic syntax is rewritten to
            // a physical specialization name; structural inference still needs Box<int>, not the
            // generated Box__g1__... spelling.
            declaredLocalType = inferExpressionType(unit, child->first(), sequentialScope);
        }
        childScopes.push_back(sequentialScope);
        if (child == nullptr || child->kind() != ast::AstNodeKind::VariableDeclarationStatement)
            continue;
        const ast::AstNode* initializer = child->first();
        if (initializer != nullptr && initializer->kind() == ast::AstNodeKind::LambdaExpression &&
            !initializer->genericParameters().empty())
        {
            sequentialScope.genericLambdas.write()[child->text()] =
                GenericLookupScope::GenericLambda{
                    initializer, child, sequentialScope.declarationContainer};
            continue;
        }
        if (declaredLocalType == nullptr && child->first() != nullptr)
            declaredLocalType = inferExpressionType(unit, child->first(), sequentialScope);
        if (declaredLocalType != nullptr)
            sequentialScope.addValueType(child->text(), declaredLocalType);
    }
    for (std::size_t index = children.size(); index > 0; --index)
        frames.push_back(Frame{children[index - 1U], currentNamespace, childOwner,
            childScopes[index - 1U], false});
    if (node->third() != nullptr)
        frames.push_back(Frame{node->third(), currentNamespace, ownerName, currentScope, false});
    if (node->second() != nullptr)
        frames.push_back(Frame{node->second(), currentNamespace, ownerName, currentScope, false});
    if (node->first() != nullptr)
        frames.push_back(Frame{node->first(), currentNamespace, ownerName, currentScope, false});
    }
}

void ProjectGenericModel::collectBodyTypeSpecializationDemand(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::string& namespaceName,
    const GenericLookupScope& scope)
{
    if (node == nullptr || node->kind() != ast::AstNodeKind::CallExpression ||
        node->auxiliaryText() != "list-pattern-rest" || node->first() == nullptr ||
        node->first()->kind() != ast::AstNodeKind::MemberExpression ||
        node->first()->first() == nullptr)
    {
        return;
    }

    ast::TypeSyntax* receiverType = inferExpressionType(unit, node->first()->first(), scope);
    if (receiverType == nullptr || receiverType->modifiers().empty()) return;

    // `in/ref/out T[N]` is represented as T[N]* for the calling convention. The synthetic
    // MatchValueExpression still denotes the array value, so look through exactly that recorded
    // reference wrapper before removing the one outer collection modifier used to derive T.
    // The pass kind remains intact in the lexical scope; definitive Sema uses `in` to select
    // ReadOnlySpan<T> and mutable passes to select Span<T>.
    std::size_t structuralModifierCount = receiverType->modifiers().size();
    if (node->first()->first()->kind() == ast::AstNodeKind::MatchValueExpression &&
        !scope.matchValuePassKind.empty() &&
        receiverType->modifiers()[structuralModifierCount - 1U].kind ==
            ast::TypeModifierSyntaxKind::Pointer)
    {
        --structuralModifierCount;
    }
    if (structuralModifierCount == 0U) return;
    const ast::TypeModifierSyntaxKind outer =
        receiverType->modifiers()[structuralModifierCount - 1U].kind;
    if (outer != ast::TypeModifierSyntaxKind::DynamicArray &&
        outer != ast::TypeModifierSyntaxKind::Array)
    {
        return;
    }

    ast::TypeSyntax* elementType =
        cloneType(unit.arena(), receiverType, {}, structuralModifierCount - 1U);
    if (elementType == nullptr) return;

    // Readonly-ness is a semantic property of the receiver (in-parameter, readonly field, const
    // local, and so on) and is deliberately not guessed by this pre-Sema pass. Materialize the
    // finite pair of canonical views. The specialization cache collapses repeated body requests,
    // while Sema selects the one that the actual receiver requires.
    for (const std::string_view templateName : {"ReadOnlySpan", "Span"})
    {
        ast::TypeSyntax* demand = unit.arena().make<ast::TypeSyntax>(
            node->range(), lexer::TokenKind::Identifier, std::string(templateName));
        demand->addTypeArgument(cloneType(unit.arena(), elementType, {}));
        std::string stableKey = std::to_string(unit.projectOrdinal()) + "|" + namespaceName + "|" +
            std::string(templateName) + "|" + typeSpelling(elementType) + "|" +
            std::to_string(node->range().begin().offset());
        const bool duplicated = !bodyTypeDemandKeys_.insert(stableKey).second;
        if (telemetry_.enabled) telemetry_.noteDemand(duplicated);
        // NSC305-G09: deduplicate demands by stableKey (skip the duplicate).
        if (duplicated) continue;
        bodyTypeSpecializationDemands_.push_back(BodyTypeSpecializationDemand{
            &unit, demand, scope, namespaceName, std::move(stableKey)});
    }
}

// NSC305-G09. Processes one wave of body-type demands (see the header).
bool ProjectGenericModel::processBodyTypeDemandWave(
    std::vector<BodyTypeSpecializationDemand>& wave,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::ranges::sort(wave, {}, &BodyTypeSpecializationDemand::stableKey);
    for (BodyTypeSpecializationDemand& demand : wave)
    {
        if (demand.unit == nullptr || demand.syntax == nullptr) continue;
        if (!processTypeSyntax(
                *demand.unit, demand.syntax, demand.namespaceName, demand.scope, diagnostics))
        {
            return false;
        }
    }
    return true;
}

bool ProjectGenericModel::materializeBodyTypeSpecializationDemands(
    diagnostics::DiagnosticEngine& diagnostics)
{
    while (!bodyTypeSpecializationDemands_.empty())
    {
        std::vector<BodyTypeSpecializationDemand> wave;
        wave.swap(bodyTypeSpecializationDemands_);
        bodyTypeDemandKeys_.clear();
        if (!processBodyTypeDemandWave(wave, diagnostics)) return false;
    }
    return !diagnostics.hasErrors();
}

// NSC305-G08. Materializes a resolved closed type and rewrites the spelling (see the header).
bool ProjectGenericModel::materializeTypeSyntax(
    ProjectSyntaxUnit& unit,
    ast::TypeSyntax* syntax,
    TemplateState& state,
    const std::string& namespaceName,
    diagnostics::DiagnosticEngine& diagnostics,
    std::string* outSpecializationKey)
{
    currentInstantiationProjectOrdinal_ = unit.projectOrdinal();
    currentInstantiationNamespace_ = namespaceName;
    // NSC-LIMIT-007: el SITIO. Un `Caja<int>` invalido tiene que sonar donde alguien lo escribio,
    // no sobre la declaracion de `Caja<T>`, que es correcta.
    currentInstantiationRange_ = syntax->range();
    std::string specializationKey;
    const std::string specialized = ensureTypeSpecialization(
        state, syntax->typeArguments(), diagnostics, &specializationKey);
    if (specialized.empty()) return false;
    if (outSpecializationKey != nullptr) *outSpecializationKey = specializationKey;

    // Keep the closed template's canonical key on this exact syntax node before erasing its
    // arguments. An outer specialization can then use that declaration-derived identity instead
    // of reinterpreting the generated name under a caller project.
    {
        std::string canonical = "generic:" + specializationKey;
        appendTypeModifiers(canonical, syntax);
        resolvedTypeIdentityBySyntax_[syntax] = std::move(canonical);
        resolvedTypeOriginBySyntax_[syntax] = DeclaredTypeOrigin{
            state.record.projectOrdinal, state.record.namespaceName};
        syntax->setOriginProjectOrdinal(state.record.projectOrdinal);
        syntax->setOriginNamespace(state.record.namespaceName);
    }
    // Preserve an explicitly written qualifier.
    const std::string originalBaseName = syntax->baseName();
    if (const std::size_t separator = originalBaseName.find_last_of('.');
        separator != std::string::npos)
    {
        syntax->setBaseName(originalBaseName.substr(0, separator + 1) + specialized);
    }
    else
    {
        syntax->setBaseName(specialized);
    }
    syntax->clearTypeArguments();
    return true;
}

// NSC305-G08. Records a type specialization demand (the discovery half of the two-pass).
void ProjectGenericModel::collectTypeSpecializationDemand(
    ProjectSyntaxUnit& unit,
    ast::TypeSyntax* target,
    TemplateState& state,
    const std::string& namespaceName)
{
    typeSpecializationDemands_.push_back(GenericTypeDemand{
        &unit,
        &state,
        {},
        target,
        {},
        {},
        {},
        InstantiationContext{
            unit.projectOrdinal(),
            namespaceName,
            target->range(),
            currentOverloadGroup_,
            activePackSubstitutions_,
            activeAssociatedProjections_}});
}

// NSC305-G08. Executes the collected type demands in waves (the apply half of the two-pass).
bool ProjectGenericModel::executeTypeDemandWaves(
    diagnostics::DiagnosticEngine& diagnostics)
{
    while (!typeSpecializationDemands_.empty())
    {
        std::vector<GenericTypeDemand> wave;
        wave.swap(typeSpecializationDemands_);
        for (GenericTypeDemand& demand : wave)
        {
            if (demand.unit == nullptr || demand.state == nullptr || demand.target == nullptr)
                continue;
            currentInstantiationProjectOrdinal_ = demand.context.projectOrdinal;
            currentInstantiationNamespace_ = demand.context.namespaceName;
            currentInstantiationRange_ = demand.context.range;
            currentOverloadGroup_ = demand.context.overloadGroup;
            activePackSubstitutions_ = demand.context.packSubstitutions;
            activeAssociatedProjections_ = demand.context.associatedProjections;
            if (!materializeTypeSyntax(
                    *demand.unit, demand.target, *demand.state, demand.context.namespaceName,
                    diagnostics))
            {
                return false;
            }
        }
    }
    return !diagnostics.hasErrors();
}

bool ProjectGenericModel::processTypeSyntax(
    ProjectSyntaxUnit& unit,
    ast::TypeSyntax* syntax,
    const std::string& namespaceName,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics,
    std::string* outSpecializationKey)
{
    if (syntax == nullptr) return true;
    std::string constType;
    std::string constValue;
    if (decodeConstGenericArgument(syntax, constType, constValue)) return true;
    if (syntax->isPackExpansion())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "generic pack expansion is valid only for a declared type pack inside a generic argument list");
        return false;
    }
    for (ast::TypeSyntax* argument : syntax->typeArguments())
    {
        if (!processTypeSyntax(unit, argument, namespaceName, scope, diagnostics)) return false;
    }
    if (syntax->isFunction())
    {
        if (!processTypeSyntax(
                unit, syntax->functionReturnType(), namespaceName, scope, diagnostics))
            return false;
        if (syntax->functionErrorType() != nullptr &&
            !processTypeSyntax(
                unit, syntax->functionErrorType(), namespaceName, scope, diagnostics))
            return false;
        return true;
    }
    // Structural surfaces still recurse into their arguments above, but never name a reusable
    // nominal template. Keep this classification shared with metadata dependency discovery.
    if (isStructuralGenericSurface(syntax)) return true;

    TypeResolution resolution = resolveTypeTemplate(
        unit, namespaceName, syntax->baseName(), syntax->typeArguments().size());
    if (resolution.kind != TypeResolutionKind::Unique)
    {
        resolution = resolveTypeTemplate(
            unit, namespaceName, syntax->baseName(), std::numeric_limits<std::size_t>::max());
    }
    TemplateState* state = resolution.kind == TypeResolutionKind::Unique ? resolution.state : nullptr;
    if (state == nullptr)
    {
        if (!syntax->typeArguments().empty())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                syntax->range(),
                "type '" + syntax->baseName() +
                    "' does not name a unique visible generic class/struct template");
            return false;
        }
        if (const DeclaredTypeIdentity* declaration = resolveDeclaredTypeIdentity(
                syntax, unit.projectOrdinal(), namespaceName, scope);
            declaration != nullptr)
        {
            std::string canonical = "type:" + declaration->identityKey;
            appendTypeModifiers(canonical, syntax);
            resolvedTypeIdentityBySyntax_[syntax] = std::move(canonical);
            resolvedTypeOriginBySyntax_[syntax] = DeclaredTypeOrigin{
                declaration->projectOrdinal, declaration->namespaceName};
        }
        return true;
    }

    if (!genericArityAccepts(state->record, syntax->typeArguments().size()))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            syntax->range(),
            "generic type '" + state->record.name + "' expects " +
                (trailingPackIndex(state->record).has_value()
                    ? "at least " + std::to_string(*trailingPackIndex(state->record))
                    : std::to_string(state->record.parameters.size())) +
                " type argument(s) but received " +
                std::to_string(syntax->typeArguments().size()));
        return false;
    }
    if (!normalizeGenericArguments(
            state->record, syntax->typeArguments(), syntax->range(), diagnostics))
        return false;

    // NS304-T08-F06: an open generic reference whose argument is a scope parameter does not
    // materialize a closed clone; record its template identity and keep its source spelling so a
    // later substitution can still replace T.
    const bool anyOpenParameter = std::ranges::any_of(
        syntax->typeArguments(), [&](const ast::TypeSyntax* argument)
        {
            return argument != nullptr && argument->typeArguments().empty() &&
                scope.typeParameters.read().contains(argument->baseName());
        });
    if (anyOpenParameter)
    {
        std::string canonical = "generic:" + state->record.canonicalKey;
        appendTypeModifiers(canonical, syntax);
        resolvedTypeIdentityBySyntax_[syntax] = std::move(canonical);
        return true;
    }

    return materializeTypeSyntax(
        unit, syntax, *state, namespaceName, diagnostics, outSpecializationKey);
}

ast::TypeSyntax* ProjectGenericModel::inferExpressionType(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* expression,
    const GenericLookupScope& scope)
{
    if (expression == nullptr) return nullptr;
    const auto builtin = [&](lexer::TokenKind kind, std::string name)
    {
        return unit.arena().make<ast::TypeSyntax>(expression->range(), kind, std::move(name));
    };
    const auto clone = [&](const ast::TypeSyntax* source)
    {
        bool conflict = false;
        return cloneAliasType(unit.arena(), source, {}, conflict);
    };

    switch (expression->kind())
    {
        case ast::AstNodeKind::IdentifierExpression:
        {
            const ast::TypeSyntax* found = scope.findValueType(expression->text());
            return found == nullptr ? nullptr : clone(found);
        }
        case ast::AstNodeKind::MatchValueExpression:
            return clone(scope.matchValueType);
        case ast::AstNodeKind::MemberExpression:
            if (expression->first() != nullptr &&
                expression->first()->kind() == ast::AstNodeKind::ThisExpression)
            {
                const ast::TypeSyntax* found = scope.findValueType(expression->text());
                return found == nullptr ? nullptr : clone(found);
            }
            return nullptr;
        case ast::AstNodeKind::IntegerLiteralExpression:
        {
            const std::string key(builtins::integerLiteralTypeKey(
                expression->text(), expression->integerValue(), false));
            if (key == "builtin:uint") return builtin(lexer::TokenKind::KwUInt, "uint");
            if (key == "builtin:long") return builtin(lexer::TokenKind::KwLong, "long");
            if (key == "builtin:ulong") return builtin(lexer::TokenKind::KwULong, "ulong");
            return builtin(lexer::TokenKind::KwInt, "int");
        }
        case ast::AstNodeKind::FloatingLiteralExpression:
            return !expression->text().empty() &&
                    (expression->text().back() == 'f' || expression->text().back() == 'F')
                ? builtin(lexer::TokenKind::KwFloat, "float")
                : builtin(lexer::TokenKind::KwDouble, "double");
        case ast::AstNodeKind::CharacterLiteralExpression:
            return builtin(lexer::TokenKind::KwChar, "char");
        case ast::AstNodeKind::StringLiteralExpression:
            return builtin(lexer::TokenKind::KwString, "string");
        case ast::AstNodeKind::BooleanLiteralExpression:
            return builtin(lexer::TokenKind::KwBool, "bool");
        case ast::AstNodeKind::CastExpression:
        case ast::AstNodeKind::SafeCastExpression:
        case ast::AstNodeKind::DefaultValueExpression:
        case ast::AstNodeKind::ObjectCreationExpression:
            return clone(expression->typeSyntax());
        case ast::AstNodeKind::PlacementConstructionExpression:
        {
            ast::TypeSyntax* result = clone(expression->typeSyntax());
            if (result != nullptr)
                result->addModifier(ast::TypeModifierSyntax{
                    ast::TypeModifierSyntaxKind::Pointer, nullptr, expression->range()});
            return result;
        }
        case ast::AstNodeKind::NewExpression:
        {
            ast::TypeSyntax* result = clone(expression->typeSyntax());
            if (result == nullptr) return nullptr;
            result->addModifier(ast::TypeModifierSyntax{
                expression->flag() ? ast::TypeModifierSyntaxKind::DynamicArray
                                   : ast::TypeModifierSyntaxKind::Pointer,
                nullptr,
                expression->range()});
            return result;
        }
        case ast::AstNodeKind::StackallocExpression:
        {
            ast::TypeSyntax* result = clone(expression->typeSyntax());
            if (result != nullptr)
                result->addModifier(ast::TypeModifierSyntax{
                    ast::TypeModifierSyntaxKind::Pointer, nullptr, expression->range()});
            return result;
        }
        case ast::AstNodeKind::MoveExpression:
            return inferExpressionType(unit, expression->first(), scope);
        case ast::AstNodeKind::UnaryExpression:
        {
            ast::TypeSyntax* operand = inferExpressionType(unit, expression->first(), scope);
            if (operand == nullptr) return nullptr;
            if (expression->tokenKind() == lexer::TokenKind::Bang)
                return builtin(lexer::TokenKind::KwBool, "bool");
            if (expression->tokenKind() == lexer::TokenKind::Amp)
            {
                operand->addModifier(ast::TypeModifierSyntax{
                    ast::TypeModifierSyntaxKind::Pointer, nullptr, expression->range()});
                return operand;
            }
            if (expression->tokenKind() != lexer::TokenKind::Star ||
                operand->modifiers().empty() ||
                operand->modifiers().back().kind != ast::TypeModifierSyntaxKind::Pointer)
                return operand;

            ast::TypeSyntax* result = unit.arena().make<ast::TypeSyntax>(
                operand->range(), operand->baseTokenKind(), operand->baseName());
            result->setOwnershipQualifier(operand->ownershipQualifier());
            for (ast::TypeSyntax* argument : operand->typeArguments())
                result->addTypeArgument(clone(argument));
            for (std::size_t index = 0; index + 1 < operand->modifiers().size(); ++index)
                result->addModifier(operand->modifiers()[index]);
            return result;
        }
        case ast::AstNodeKind::AssignmentExpression:
            return inferExpressionType(unit, expression->second(), scope);
        case ast::AstNodeKind::ConditionalExpression:
        {
            ast::TypeSyntax* whenTrue = inferExpressionType(unit, expression->second(), scope);
            ast::TypeSyntax* whenFalse = inferExpressionType(unit, expression->third(), scope);
            // A recursive generic lambda has no callable return entry until its first closed
            // body is inferred. One arm can still provide the fixed point (`n == 0 ? 0 : self`).
            // Keep the ordinary exact-equality rule when both arms are known.
            if (whenTrue == nullptr) return whenFalse;
            if (whenFalse == nullptr) return whenTrue;
            if (typeSpelling(whenTrue) != typeSpelling(whenFalse)) return nullptr;
            return whenTrue;
        }
        case ast::AstNodeKind::BinaryExpression:
        {
            switch (expression->tokenKind())
            {
                case lexer::TokenKind::EqualEqual:
                case lexer::TokenKind::BangEqual:
                case lexer::TokenKind::Less:
                case lexer::TokenKind::LessEqual:
                case lexer::TokenKind::Greater:
                case lexer::TokenKind::GreaterEqual:
                case lexer::TokenKind::AmpAmp:
                case lexer::TokenKind::PipePipe:
                    return builtin(lexer::TokenKind::KwBool, "bool");
                default: break;
            }
            ast::TypeSyntax* left = inferExpressionType(unit, expression->first(), scope);
            ast::TypeSyntax* right = inferExpressionType(unit, expression->second(), scope);
            if (left == nullptr) return right;
            if (right == nullptr) return left;
            if (typeSpelling(left) != typeSpelling(right)) return nullptr;
            return left;
        }
        case ast::AstNodeKind::TupleExpression:
        {
            ast::TypeSyntax* tuple = unit.arena().make<ast::TypeSyntax>(
                expression->range(), lexer::TokenKind::Invalid, "$tuple");
            tuple->setTuple();
            for (const ast::AstNode* element : expression->children())
            {
                if (element == nullptr ||
                    element->kind() == ast::AstNodeKind::SpreadElementExpression)
                    return nullptr;
                ast::TypeSyntax* elementType = inferExpressionType(unit, element, scope);
                if (elementType == nullptr) return nullptr;
                tuple->addTupleElement(elementType);
            }
            return tuple;
        }
        case ast::AstNodeKind::CallExpression:
        {
            const auto found = inferredCallReturnTypes_.find(expression);
            if (found != inferredCallReturnTypes_.end()) return clone(found->second);
            const auto ordinary = ordinaryCallableReturnTypes_.find(
                methodCalleeName(expression));
            return ordinary == ordinaryCallableReturnTypes_.end()
                ? nullptr : clone(ordinary->second);
        }
        default:
            return nullptr;
    }
}

bool ProjectGenericModel::inferTypeArguments(
    ProjectSyntaxUnit& unit,
    const ast::TypeSyntax* pattern,
    const ast::TypeSyntax* actual,
    const GenericTemplateRecord& record,
    const std::unordered_set<std::string>& parameters,
    std::unordered_map<std::string, ast::TypeSyntax*>& inferred)
{
    if (pattern == nullptr || actual == nullptr) return false;
    if (actual->modifiers().size() < pattern->modifiers().size()) return false;
    const std::size_t modifierPrefix = actual->modifiers().size() - pattern->modifiers().size();
    for (std::size_t index = 0; index < pattern->modifiers().size(); ++index)
    {
        const ast::TypeModifierSyntax& patternModifier = pattern->modifiers()[index];
        const ast::TypeModifierSyntax& actualModifier =
            actual->modifiers()[modifierPrefix + index];
        if (patternModifier.kind != actualModifier.kind)
            return false;
        if (patternModifier.kind != ast::TypeModifierSyntaxKind::Array ||
            patternModifier.arrayCount == nullptr ||
            patternModifier.arrayCount->kind() != ast::AstNodeKind::IdentifierExpression)
            continue;

        const std::string& parameterName = patternModifier.arrayCount->text();
        const auto parameter = std::ranges::find_if(
            record.parameterSyntaxes,
            [&](const ast::GenericParameterSyntax& candidate)
            {
                return candidate.name == parameterName &&
                    candidate.kind == ast::GenericParameterSyntaxKind::Constant;
            });
        if (parameter == record.parameterSyntaxes.end() || actualModifier.arrayCount == nullptr)
            continue;

        const ProjectIntegerEvaluator evaluator(nullptr);
        const std::optional<ProjectIntegerValue> extent =
            evaluator.evaluate(actualModifier.arrayCount);
        if (!extent.has_value()) return false;
        ast::TypeSyntax* value = unit.arena().make<ast::TypeSyntax>(
            actualModifier.arrayCount->range(), lexer::TokenKind::Invalid,
            language::constGenericArgumentMarker(
                extent->typeKey, std::to_string(extent->bits)));
        const auto prior = inferred.find(parameterName);
        if (prior == inferred.end()) inferred.emplace(parameterName, value);
        else if (typeSpelling(prior->second) != typeSpelling(value)) return false;
    }

    const auto constantParameter = std::ranges::find_if(
        record.parameterSyntaxes,
        [&](const ast::GenericParameterSyntax& candidate)
        {
            return candidate.name == pattern->baseName() &&
                candidate.kind == ast::GenericParameterSyntaxKind::Constant;
        });
    if (parameters.contains(pattern->baseName()) &&
        constantParameter != record.parameterSyntaxes.end() &&
        pattern->typeArguments().empty() && pattern->modifiers().empty())
    {
        std::string actualType;
        std::string actualValue;
        if (!decodeConstGenericArgument(actual, actualType, actualValue)) return false;
        ast::TypeSyntax* value = unit.arena().make<ast::TypeSyntax>(
            actual->range(), lexer::TokenKind::Invalid,
            language::constGenericArgumentMarker(actualType, actualValue));
        const auto prior = inferred.find(pattern->baseName());
        if (prior == inferred.end())
        {
            inferred.emplace(pattern->baseName(), value);
            return true;
        }
        return typeSpelling(prior->second) == typeSpelling(value);
    }

    const auto typeParameter = std::ranges::find_if(
        record.parameterSyntaxes,
        [&](const ast::GenericParameterSyntax& candidate)
        {
            return candidate.name == pattern->baseName() &&
                (candidate.kind == ast::GenericParameterSyntaxKind::Type ||
                 candidate.kind == ast::GenericParameterSyntaxKind::TypePack);
        });
    if (parameters.contains(pattern->baseName()) &&
        typeParameter != record.parameterSyntaxes.end() && pattern->typeArguments().empty())
    {
        ast::TypeSyntax* value = unit.arena().make<ast::TypeSyntax>(
            actual->range(), actual->baseTokenKind(), actual->baseName());
        if (pattern->ownershipQualifier() == ast::OwnershipQualifierSyntaxKind::None)
            value->setOwnershipQualifier(actual->ownershipQualifier());
        value->setTuple(actual->isTuple());
        value->setDynamicTrait(actual->isDynamicTrait());
        value->setOpaqueResult(actual->isOpaqueResult());
        value->setOpaqueResultTraitName(actual->opaqueResultTraitName());
        value->setPointeeConst(actual->isPointeeConst());
        if (actual->originProjectOrdinal().has_value())
            value->setOriginProjectOrdinal(*actual->originProjectOrdinal());
        if (actual->originNamespace().has_value())
            value->setOriginNamespace(*actual->originNamespace());
        bool conflict = false;
        for (std::size_t index = 0; index < actual->typeArguments().size(); ++index)
        {
            ast::TypeSyntax* argument = cloneAliasType(
                unit.arena(), actual->typeArguments()[index], {}, conflict);
            if (actual->isTuple())
            {
                const std::string name = index < actual->tupleElementNames().size()
                    ? actual->tupleElementNames()[index] : std::string{};
                value->addTupleElement(argument, name);
            }
            else
            {
                value->addTypeArgument(argument);
            }
        }
        for (std::size_t index = 0; index < modifierPrefix; ++index)
            value->addModifier(actual->modifiers()[index]);
        const auto prior = inferred.find(pattern->baseName());
        if (prior == inferred.end())
        {
            inferred.emplace(pattern->baseName(), value);
            return true;
        }
        return typeSpelling(prior->second) == typeSpelling(value);
    }

    if (modifierPrefix != 0 || pattern->baseTokenKind() != actual->baseTokenKind() ||
        pattern->baseName() != actual->baseName() ||
        pattern->typeArguments().size() != actual->typeArguments().size())
        return false;
    for (std::size_t index = 0; index < pattern->typeArguments().size(); ++index)
    {
        if (!inferTypeArguments(
                unit, pattern->typeArguments()[index], actual->typeArguments()[index],
                record, parameters, inferred)) return false;
    }
    return true;
}

bool ProjectGenericModel::normalizeGenericArguments(
    const GenericTemplateRecord& record,
    std::span<ast::TypeSyntax* const> arguments,
    SourceRange range,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::optional<std::size_t> packIndex = trailingPackIndex(record);
    if (packIndex.has_value())
    {
        const std::array<std::size_t, 1> lengths{
            arguments.size() - *packIndex};
        std::string packError;
        if (!language::validatePackExpansion(
                lengths, DefaultMaximumPackExpansionCount, packError))
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidArgumentCount,
                diagnostics::DiagnosticSeverity::Error,
                range,
                packError + " (limit " +
                    std::to_string(DefaultMaximumPackExpansionCount) + ")");
            return false;
        }
    }
    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        const std::size_t parameterIndex =
            packIndex.has_value() && index >= *packIndex ? *packIndex : index;
        const ast::GenericParameterSyntaxKind kind =
            parameterIndex < record.parameterSyntaxes.size()
            ? record.parameterSyntaxes[parameterIndex].kind
            : ast::GenericParameterSyntaxKind::Type;
        const ast::GenericParameterSyntax& parameter = record.parameterSyntaxes[parameterIndex];
        const std::string constSubject = kind == ast::GenericParameterSyntaxKind::ConstantPack
            ? "const generic pack '" : "const generic parameter '";
        std::string expectedType = constGenericParameterTypeKey(parameter);
        if (expectedType.empty() && parameter.constantType != nullptr)
        {
            const std::function<std::string(const ast::TypeSyntax*)> structuralTypeKey =
                [&](const ast::TypeSyntax* type) -> std::string
                {
                    if (type == nullptr || !type->modifiers().empty()) return {};
                    if (lexer::isBuiltinTypeKeyword(type->baseTokenKind()))
                        return "builtin:" + type->baseName();
                    if (type->isTuple())
                    {
                        std::string key = "tuple<";
                        for (std::size_t item = 0; item < type->typeArguments().size(); ++item)
                        {
                            const std::string element = structuralTypeKey(type->typeArguments()[item]);
                            if (element.empty()) return {};
                            if (item != 0) key += ',';
                            key += element;
                        }
                        return key + '>';
                    }
                    const auto enumType = constEnumTypeBySpelling_.find(type->baseName());
                    return enumType != constEnumTypeBySpelling_.end()
                        ? enumType->second : std::string{};
                };
            expectedType = structuralTypeKey(parameter.constantType);
        }
        std::string argumentType;
        std::string argumentValue;
        bool constant = decodeConstGenericArgument(
                arguments[index], argumentType, argumentValue) ||
            normalizeConstExpressionArgument(arguments[index], argumentType, argumentValue);
        if (!constant && !expectedType.empty() && expectedType.starts_with("enum:") &&
            arguments[index] != nullptr)
        {
            const auto enumValue = constEnumValueBySpelling_.find(arguments[index]->baseName());
            if (enumValue != constEnumValueBySpelling_.end() &&
                !enumValue->second.first.empty())
            {
                argumentType = enumValue->second.first;
                argumentValue = enumValue->second.second;
                arguments[index]->setBaseName(language::constGenericArgumentMarker(
                    argumentType, argumentValue));
                constant = true;
            }
        }
        if (kind == ast::GenericParameterSyntaxKind::Type)
        {
            if (!constant) continue;
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                arguments[index]->range(),
                "generic type parameter '" + record.parameters[parameterIndex] +
                    "' requires a type argument, not a compile-time value");
            return false;
        }
        if (kind == ast::GenericParameterSyntaxKind::TypePack)
        {
            if (!constant) continue;
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                arguments[index]->range(),
                "generic type pack '" + record.parameters[parameterIndex] +
                    "' cannot contain a compile-time value argument");
            return false;
        }
        if (!constant)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                arguments[index] != nullptr ? arguments[index]->range() : range,
                constSubject + record.parameters[parameterIndex] +
                    "' requires a compile-time value argument");
            return false;
        }

        if (expectedType.empty())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                parameter.constantType != nullptr ? parameter.constantType->range() : range,
                constSubject + record.parameters[parameterIndex] +
                    "' has a type that is not an enum or approved structural const type");
            return false;
        }
        if (expectedType.starts_with("enum:"))
        {
            if (argumentType != expectedType)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    arguments[index]->range(),
                    constSubject + record.parameters[parameterIndex] +
                        "' requires a value of enum type '" + parameter.constantType->baseName() + "'");
                return false;
            }
            continue;
        }
        if (expectedType.starts_with("tuple<"))
        {
            if (argumentType != expectedType)
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    arguments[index]->range(),
                    constSubject + record.parameters[parameterIndex] +
                        "' requires structural value type '" + expectedType + "'");
                return false;
            }
            continue;
        }
        if (expectedType == "builtin:bool")
        {
            if (argumentType != "builtin:bool" ||
                (argumentValue != "true" && argumentValue != "false"))
            {
                diagnostics.report(
                    diagnostics::DiagnosticId::InvalidType,
                    diagnostics::DiagnosticSeverity::Error,
                    arguments[index]->range(),
                    constSubject + record.parameters[parameterIndex] +
                        "' requires a boolean value");
                return false;
            }
            continue;
        }
        if (argumentType == "builtin:bool")
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                arguments[index]->range(),
                constSubject + record.parameters[parameterIndex] +
                    "' requires an integral value");
            return false;
        }

        std::uint64_t value = 0;
        const auto parsed = std::from_chars(
            argumentValue.data(), argumentValue.data() + argumentValue.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != argumentValue.data() + argumentValue.size())
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                arguments[index]->range(),
                "const generic argument is not a normalized integral value");
            return false;
        }
        const unsigned expectedWidth = ProjectIntegerEvaluator::integerWidth(expectedType);
        const bool expectedSigned = ProjectIntegerEvaluator::isSignedInteger(expectedType);
        const bool argumentSigned = ProjectIntegerEvaluator::isSignedInteger(argumentType);
        bool fits = expectedWidth != 0;
        if (fits && expectedSigned)
        {
            const std::uint64_t positiveMaximum = expectedWidth == 64
                ? 0x7fffffffffffffffULL
                : ((std::uint64_t{1} << (expectedWidth - 1U)) - 1U);
            if (argumentSigned && static_cast<std::int64_t>(value) < 0)
            {
                const std::int64_t minimum = expectedWidth == 64
                    ? (std::numeric_limits<std::int64_t>::min)()
                    : -(std::int64_t{1} << (expectedWidth - 1U));
                fits = static_cast<std::int64_t>(value) >= minimum;
            }
            else
            {
                fits = value <= positiveMaximum;
            }
        }
        else if (fits)
        {
            const std::uint64_t maximum = expectedWidth == 64
                ? (std::numeric_limits<std::uint64_t>::max)()
                : ((std::uint64_t{1} << expectedWidth) - 1U);
            fits = (!argumentSigned || static_cast<std::int64_t>(value) >= 0) &&
                value <= maximum;
        }
        if (!fits)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidType,
                diagnostics::DiagnosticSeverity::Error,
                arguments[index]->range(),
                "const generic argument '" + argumentValue + "' does not fit parameter type '" +
                    (parameter.constantType != nullptr ? parameter.constantType->baseName() : "<missing>") + "'");
            return false;
        }
        // The declaration type, not the literal suffix, participates in identity. Therefore
        // `Vector<int, 4>` and `Vector<int, 4u>` share the same specialization when N is uint.
        const std::uint64_t normalized = ProjectIntegerEvaluator::normalize(value, expectedType);
        const std::string normalizedText = expectedSigned &&
                static_cast<std::int64_t>(normalized) < 0
            ? std::to_string(static_cast<std::int64_t>(normalized))
            : std::to_string(normalized);
        arguments[index]->setBaseName(language::constGenericArgumentMarker(
            expectedType, normalizedText));
    }
    return true;
}

bool ProjectGenericModel::applyExpectedTypeToExpression(
    ProjectSyntaxUnit& unit,
    ast::AstNode* expression,
    const ast::TypeSyntax* expectedType,
    const std::string& namespaceName,
    const std::string& ownerName,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics,
    std::size_t inferenceDepth)
{
    if (expression == nullptr || expectedType == nullptr ||
        inferenceDepth > DefaultMaximumInstantiationDepth)
        return false;

    if (expression->kind() == ast::AstNodeKind::CallExpression)
    {
        if (!expression->typeSyntaxes().empty() || expression->flag()) return true;
        if (!inferGenericCall(
                unit, expression, namespaceName, ownerName, scope, diagnostics, expectedType,
                true, inferenceDepth + 1U))
            return false;
        if (!expression->typeSyntaxes().empty() || expression->flag())
            return processGenericCall(
                unit, expression, namespaceName, ownerName, scope, diagnostics);
        return true;
    }

    if (expression->kind() == ast::AstNodeKind::ConditionalExpression)
    {
        const bool whenTrue = applyExpectedTypeToExpression(
            unit, expression->second(), expectedType, namespaceName, ownerName, scope,
            diagnostics, inferenceDepth + 1U);
        const bool whenFalse = applyExpectedTypeToExpression(
            unit, expression->third(), expectedType, namespaceName, ownerName, scope,
            diagnostics, inferenceDepth + 1U);
        return whenTrue && whenFalse;
    }

    if (expression->kind() == ast::AstNodeKind::TupleExpression && expectedType->isTuple() &&
        expression->children().size() == expectedType->typeArguments().size())
    {
        bool valid = true;
        for (std::size_t index = 0; index < expression->children().size(); ++index)
        {
            ast::AstNode* element = expression->children()[index];
            if (element == nullptr || element->kind() == ast::AstNodeKind::SpreadElementExpression)
            {
                valid = false;
                continue;
            }
            valid = applyExpectedTypeToExpression(
                unit, element, expectedType->typeArguments()[index], namespaceName, ownerName,
                scope, diagnostics, inferenceDepth + 1U) && valid;
        }
        return valid;
    }

    if (expression->kind() == ast::AstNodeKind::MoveExpression)
        return applyExpectedTypeToExpression(
            unit, expression->first(), expectedType, namespaceName, ownerName, scope,
            diagnostics, inferenceDepth + 1U);

    return true;
}

bool ProjectGenericModel::inferLambdaTypeArguments(
    ProjectSyntaxUnit& unit,
    const ast::AstNode* lambda,
    const ast::TypeSyntax* delegatePattern,
    const GenericTemplateRecord& methodRecord,
    const std::unordered_set<std::string>& methodParameters,
    const GenericLookupScope& scope,
    std::unordered_map<std::string, ast::TypeSyntax*>& inferred)
{
    if (lambda == nullptr || lambda->kind() != ast::AstNodeKind::LambdaExpression ||
        delegatePattern == nullptr || delegatePattern->modifiers().size() != 0U)
        return true;

    TemplateState* delegate = nullptr;
    for (TemplateState& state : templateStates_)
    {
        if (state.record.kind != GenericTemplateKind::Delegate ||
            state.record.parameters.size() != delegatePattern->typeArguments().size() ||
            !templateVisible(unit.projectOrdinal(), state.record))
            continue;
        const std::string qualified = qualifiedName(
            state.record.namespaceName, state.record.qualifiedTypeName);
        if (state.record.name != delegatePattern->baseName() &&
            qualified != delegatePattern->baseName())
            continue;
        if (delegate != nullptr) return true;
        delegate = &state;
    }
    if (delegate == nullptr || delegate->syntax == nullptr) return true;

    std::vector<const ast::AstNode*> delegateParameters;
    for (const ast::AstNode* child : delegate->syntax->children())
        if (child != nullptr && child->kind() == ast::AstNodeKind::ParameterDeclaration)
            delegateParameters.push_back(child);
    if (delegateParameters.size() != lambda->children().size()) return false;

    std::unordered_map<std::string, const ast::TypeSyntax*> delegateArguments;
    for (std::size_t index = 0; index < delegate->record.parameters.size(); ++index)
        delegateArguments.emplace(
            delegate->record.parameters[index], delegatePattern->typeArguments()[index]);

    GenericLookupScope lambdaScope = scope;
    for (std::size_t index = 0; index < lambda->children().size(); ++index)
    {
        const ast::AstNode* lambdaParameter = lambda->children()[index];
        bool conflict = false;
        ast::TypeSyntax* signaturePattern = cloneAliasType(
            unit.arena(), delegateParameters[index]->typeSyntax(), delegateArguments, conflict);
        if (conflict || signaturePattern == nullptr) return false;
        if (lambdaParameter != nullptr && lambdaParameter->typeSyntax() != nullptr)
        {
            if (!inferTypeArguments(
                    unit, signaturePattern, lambdaParameter->typeSyntax(), methodRecord,
                    methodParameters, inferred))
                return false;
            lambdaScope.addValueType(lambdaParameter->text(), lambdaParameter->typeSyntax());
        }
    }

    if (!lambda->flag() && lambda->first() != nullptr && delegate->syntax->typeSyntax() != nullptr)
    {
        std::unordered_map<std::string, const ast::TypeSyntax*> methodArguments;
        for (const auto& [name, type] : inferred) methodArguments.emplace(name, type);
        bool conflict = false;
        ast::TypeSyntax* returnPattern = cloneAliasType(
            unit.arena(), delegate->syntax->typeSyntax(), delegateArguments, conflict);
        if (conflict || returnPattern == nullptr) return false;
        returnPattern = cloneAliasType(
            unit.arena(), returnPattern, methodArguments, conflict);
        ast::TypeSyntax* bodyType = inferExpressionType(unit, lambda->first(), lambdaScope);
        if (!conflict && bodyType != nullptr &&
            !inferTypeArguments(
                unit, returnPattern, bodyType, methodRecord, methodParameters, inferred))
            return false;
    }
    return true;
}

bool ProjectGenericModel::inferGenericCall(
    ProjectSyntaxUnit& unit,
    ast::AstNode* call,
    const std::string& namespaceName,
    const std::string& ownerName,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics,
    const ast::TypeSyntax* expectedType,
    bool reportFailure,
    std::size_t inferenceDepth)
{
    if (call == nullptr || call->kind() != ast::AstNodeKind::CallExpression ||
        !call->typeSyntaxes().empty()) return true;
    if (inferenceDepth > DefaultMaximumInstantiationDepth)
    {
        if (reportFailure)
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                call->range(),
                "generic inference exceeded the bounded expression depth (" +
                    std::to_string(DefaultMaximumInstantiationDepth) +
                    "); write type arguments explicitly");
        return false;
    }
    ast::AstNode* callee = methodCalleeSyntax(call);
    const std::string methodName = methodCalleeName(call);
    if (callee == nullptr || methodName.empty()) return true;

    std::string requestedOwner = templateSourceSpelling(namespaceName, ownerName);
    if (callee->kind() == ast::AstNodeKind::MemberExpression)
    {
        const std::string written = receiverSourceSpelling(callee->first());
        if (!written.empty()) requestedOwner = written;
    }
    QualifiedNameSiteContext siteContext = makeQualifiedNameSite(namespaceName, ownerName, scope);
    const QualifiedNameSite& site = siteContext.site;
    constexpr std::size_t kAny = std::numeric_limits<std::size_t>::max();
    const std::vector<TemplateState*> named = templatesNamedBy(
        unit, site, requestedOwner, methodName, kAny, kAny);
    // NS304-T08-F06: an extension member access has a VALUE receiver, not an owner. When the
    // receiver spelling finds no template, the extension is a namespace-level declaration, so
    // retry with the lexical namespace as the owner.
    std::vector<TemplateState*> extensionNamed;
    if (named.empty() && callee->kind() == ast::AstNodeKind::MemberExpression)
        extensionNamed = templatesNamedBy(
            unit, site, templateSourceSpelling(namespaceName, {}), methodName, kAny, kAny);
    const std::vector<TemplateState*>& effectiveNamed = named.empty() ? extensionNamed : named;
    if (effectiveNamed.empty()) return true;

    // First collect bottom-up evidence without diagnosing a missing context. A later candidate may
    // provide an exact parameter type to the inner call (for example `int x = Id(Make())`).
    // The source AST is finite and inferenceDepth applies the same public bound as materialization.
    for (ast::AstNode* argument : call->children())
    {
        if (argument == nullptr || argument->kind() != ast::AstNodeKind::CallExpression ||
            (!argument->typeSyntaxes().empty() || argument->flag())) continue;
        inferGenericCall(
            unit, argument, namespaceName, ownerName, scope, diagnostics, nullptr, false,
            inferenceDepth + 1U);
        if (!argument->typeSyntaxes().empty())
            processGenericCall(unit, argument, namespaceName, ownerName, scope, diagnostics);
    }

    // A potentially applicable non-generic overload remains an ordinary overload-resolution
    // problem. An exact expected type can, however, prove an overload irrelevant by return type;
    // adding such an unrelated declaration must not disable otherwise deterministic inference.
    for (const TemplateState* candidate : effectiveNamed)
    {
        if (candidate == nullptr || candidate->parent == nullptr) continue;
        for (const ast::AstNode* sibling : candidate->parent->children())
        {
            if (sibling != nullptr && sibling->kind() == ast::AstNodeKind::MethodDeclaration &&
                sibling->genericParameters().empty() && sibling->text() == methodName &&
                valueParameterCount(sibling) == call->children().size())
            {
                if (expectedType == nullptr || sibling->typeSyntax() == nullptr ||
                    typeSpelling(sibling->typeSyntax()) == typeSpelling(expectedType))
                    return true;
            }
        }
    }

    struct Inference final
    {
        TemplateState* candidate = nullptr;
        std::vector<ast::TypeSyntax*> arguments;
        std::size_t specificity = 0;
    };
    const auto patternSpecificity = [&](auto&& self, const ast::TypeSyntax* pattern,
                                        const std::unordered_set<std::string>& parameters)
        -> std::size_t
    {
        if (pattern == nullptr) return 0;
        std::size_t score = parameters.contains(pattern->baseName()) &&
                pattern->typeArguments().empty()
            ? 0U : 4U;
        score += pattern->modifiers().size() * 2U;
        for (const ast::TypeSyntax* argument : pattern->typeArguments())
            score += self(self, argument, parameters);
        return score;
    };
    std::vector<Inference> successes;
    for (TemplateState* candidate : effectiveNamed)
    {
        if (candidate == nullptr || candidate->syntax == nullptr) continue;
        std::vector<const ast::AstNode*> parameters;
        for (const ast::AstNode* child : candidate->syntax->children())
            if (child != nullptr && child->kind() == ast::AstNodeKind::ParameterDeclaration)
                parameters.push_back(child);

        std::vector<std::optional<std::size_t>> mapping(call->children().size());
        std::vector<bool> used(parameters.size(), false);
        std::size_t nextPositional = 0;
        bool viable = true;
        for (std::size_t argumentIndex = 0; argumentIndex < call->children().size(); ++argumentIndex)
        {
            const ast::AstNode* argument = call->children()[argumentIndex];
            std::optional<std::size_t> parameterIndex;
            if (argument != nullptr && !argument->argumentName().empty())
            {
                for (std::size_t index = 0; index < parameters.size(); ++index)
                    if (parameters[index]->text() == argument->argumentName())
                    {
                        parameterIndex = index;
                        break;
                    }
            }
            else
            {
                while (nextPositional < used.size() && used[nextPositional]) ++nextPositional;
                if (nextPositional < used.size()) parameterIndex = nextPositional++;
            }
            if (!parameterIndex.has_value() || used[*parameterIndex])
            {
                viable = false;
                break;
            }
            used[*parameterIndex] = true;
            mapping[argumentIndex] = parameterIndex;
        }
        // NS304-T08-F06: the extension receiver (first parameter, prepended as `ns3-extension`) is
        // not a call argument, so it must not trigger the "missing argument" check.
        const bool extensionCandidate =
            candidate->syntax->auxiliaryText().find("ns3-extension") != std::string::npos;
        for (std::size_t index = 0; viable && index < parameters.size(); ++index)
            if (!used[index] && parameters[index]->first() == nullptr &&
                !(extensionCandidate && index == 0))
                viable = false;
        if (!viable) continue;

        const std::unordered_set<std::string> genericParameters(
            candidate->record.parameters.begin(), candidate->record.parameters.end());
        std::unordered_map<std::string, ast::TypeSyntax*> inferred;
        for (std::size_t index = 0; viable && index < call->children().size(); ++index)
        {
            ast::TypeSyntax* actual = inferExpressionType(unit, call->children()[index], scope);
            const ast::TypeSyntax* pattern = parameters[*mapping[index]]->typeSyntax();
            // `null`, an unresolved member, or another expression without a local type supplies no
            // evidence. It is not a contradiction: an exact expected result may still infer T.
            if (actual != nullptr)
            {
                viable = inferTypeArguments(
                    unit, pattern, actual, candidate->record, genericParameters, inferred);
            }
            else if (call->children()[index] != nullptr &&
                     call->children()[index]->kind() == ast::AstNodeKind::LambdaExpression)
            {
                viable = inferLambdaTypeArguments(
                    unit, call->children()[index], pattern, candidate->record,
                    genericParameters, scope, inferred);
            }
        }
        // NS304-T08-F06: an extension receiver supplies the inference evidence when the call has no
        // explicit arguments (`b.Twice()` on `Box<I>` for `Twice<T>` over `Box<T>`).
        if (viable && call->children().empty() && call->first() != nullptr &&
            call->first()->kind() == ast::AstNodeKind::MemberExpression &&
            call->first()->first() != nullptr && !parameters.empty() && extensionCandidate)
        {
            ast::TypeSyntax* receiverActual =
                inferExpressionType(unit, call->first()->first(), scope);
            if (receiverActual != nullptr && parameters.front()->typeSyntax() != nullptr)
            {
                viable = inferTypeArguments(
                    unit, parameters.front()->typeSyntax(), receiverActual,
                    candidate->record, genericParameters, inferred);
            }
        }
        if (viable && expectedType != nullptr && candidate->syntax->typeSyntax() != nullptr)
            viable = inferTypeArguments(
                unit, candidate->syntax->typeSyntax(), expectedType,
                candidate->record, genericParameters, inferred);

        // With one visible generic template there is no candidate-state leak: any parameter type
        // made fully concrete by the first pass is safe context for an unresolved nested
        // expression. This is the top-down half of the ordered bidirectional algorithm.
        if (viable && effectiveNamed.size() == 1U)
        {
            const std::function<bool(const ast::TypeSyntax*)> containsOpenParameter =
                [&](const ast::TypeSyntax* syntax)
                {
                    if (syntax == nullptr) return false;
                    if (genericParameters.contains(syntax->baseName()) &&
                        syntax->typeArguments().empty())
                        return true;
                    return std::ranges::any_of(
                        syntax->typeArguments(), containsOpenParameter);
                };
            std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
            for (const auto& [name, type] : inferred) substitutions.emplace(name, type);
            for (std::size_t index = 0; viable && index < call->children().size(); ++index)
            {
                if (inferExpressionType(unit, call->children()[index], scope) != nullptr) continue;
                bool conflict = false;
                ast::TypeSyntax* contextualParameter = cloneAliasType(
                    unit.arena(), parameters[*mapping[index]]->typeSyntax(), substitutions,
                    conflict);
                if (conflict || contextualParameter == nullptr ||
                    containsOpenParameter(contextualParameter))
                    continue;
                if (!applyExpectedTypeToExpression(
                        unit, call->children()[index], contextualParameter, namespaceName,
                        ownerName, scope, diagnostics, inferenceDepth + 1U))
                {
                    viable = false;
                    break;
                }
                ast::TypeSyntax* actual = inferExpressionType(
                    unit, call->children()[index], scope);
                if (actual != nullptr)
                    viable = inferTypeArguments(
                        unit, parameters[*mapping[index]]->typeSyntax(), actual,
                        candidate->record, genericParameters, inferred);
            }
        }
        if (!viable || inferred.size() != candidate->record.parameters.size()) continue;
        Inference success;
        success.candidate = candidate;
        for (const ast::AstNode* parameter : parameters)
            success.specificity +=
                patternSpecificity(patternSpecificity, parameter->typeSyntax(), genericParameters);
        for (const std::string& parameter : candidate->record.parameters)
            success.arguments.push_back(inferred.at(parameter));
        successes.push_back(std::move(success));
    }

    if (successes.empty())
    {
        if (reportFailure)
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                call->range(),
                "type arguments for generic method '" + methodName +
                    "' cannot be inferred from the local call arguments or exact expected type; "
                    "write them explicitly");
        return false;
    }
    const std::size_t bestSpecificity = std::ranges::max_element(
        successes, {}, &Inference::specificity)->specificity;
    std::erase_if(successes, [&](const Inference& inference)
    {
        return inference.specificity != bestSpecificity;
    });
    const auto sameArguments = [&](const Inference& candidate)
    {
        if (candidate.arguments.size() != successes.front().arguments.size()) return false;
        for (std::size_t index = 0; index < candidate.arguments.size(); ++index)
            if (typeSpelling(candidate.arguments[index]) !=
                typeSpelling(successes.front().arguments[index])) return false;
        return true;
    };
    std::string returnSpelling;
    ast::TypeSyntax* inferredReturn = nullptr;
    bool commonReturn = true;
    for (const Inference& success : successes)
    {
        std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
        for (std::size_t index = 0; index < success.arguments.size(); ++index)
            substitutions.emplace(success.candidate->record.parameters[index], success.arguments[index]);
        bool conflict = false;
        ast::TypeSyntax* candidateReturn = cloneAliasType(
            unit.arena(), success.candidate->syntax->typeSyntax(), substitutions, conflict);
        const std::string spelling = typeSpelling(candidateReturn);
        if (inferredReturn == nullptr)
        {
            inferredReturn = candidateReturn;
            returnSpelling = spelling;
        }
        else if (spelling != returnSpelling)
        {
            commonReturn = false;
        }
    }
    if (commonReturn && inferredReturn != nullptr)
        inferredCallReturnTypes_[call] = inferredReturn;

    if (!std::ranges::all_of(successes, sameArguments))
    {
        if (reportFailure)
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidCall,
                diagnostics::DiagnosticSeverity::Error,
                call->range(),
                "generic method call '" + methodName +
                    "' has equally specific but incompatible type inferences; write type arguments explicitly");
        return false;
    }

    for (ast::TypeSyntax* argument : successes.front().arguments)
    {
        bool conflict = false;
        call->addTypeSyntax(cloneAliasType(unit.arena(), argument, {}, conflict));
    }
    return true;
}

// NSC305-G08. Materializes every shared-overload candidate of a generic call (see the header).
bool ProjectGenericModel::materializeSharedOverloads(
    ProjectSyntaxUnit& unit,
    ast::AstNode* call,
    ast::AstNode* callee,
    const std::string& methodName,
    const std::string& namespaceName,
    const std::vector<TemplateState*>& shared,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::string sharedName = methodName + "__g" +
        std::to_string(call->typeSyntaxes().size());
    for (const ast::TypeSyntax* argument : call->typeSyntaxes())
        sharedName += "__" + sanitize(typeSpelling(argument));

    currentInstantiationProjectOrdinal_ = unit.projectOrdinal();
    currentInstantiationNamespace_ = namespaceName;
    currentInstantiationRange_ = call->range();
    // NSC-LIMIT-007: las que salen de aqui son ALTERNATIVAS entre si, no obligaciones
    // simultaneas. Se marcan con un grupo comun para que la validacion de constraints exija
    // que fallen todas antes de decir nada.
    currentOverloadGroup_ = ++nextOverloadGroup_;
    for (TemplateState* candidate : shared)
    {
        if (!genericArityAccepts(candidate->record, call->typeSyntaxes().size())) continue;
        if (!normalizeGenericArguments(
                candidate->record, call->typeSyntaxes(), call->range(), diagnostics))
        {
            currentOverloadGroup_ = 0;
            return false;
        }
        if (ensureMethodSpecialization(
                *candidate, call->typeSyntaxes(), diagnostics, sharedName).empty())
        {
            currentOverloadGroup_ = 0;
            return false;
        }
    }
    currentOverloadGroup_ = 0;
    if (callee != nullptr) callee->setText(sharedName);
    call->clearTypeSyntaxes();
    call->setFlag(false);
    return true;
}

// NSC305-G08. Normalizes and materializes the single resolved method (see the header).
bool ProjectGenericModel::materializeUniqueMethod(
    ProjectSyntaxUnit& unit,
    ast::AstNode* call,
    ast::AstNode* callee,
    TemplateState& state,
    const std::string& namespaceName,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (!normalizeGenericArguments(
            state.record, call->typeSyntaxes(), call->range(), diagnostics))
        return false;

    currentInstantiationProjectOrdinal_ = unit.projectOrdinal();
    currentInstantiationNamespace_ = namespaceName;
    currentInstantiationRange_ = call->range();
    const std::string specialized = ensureMethodSpecialization(
        state, call->typeSyntaxes(), diagnostics);
    if (specialized.empty()) return false;
    if (callee != nullptr) callee->setText(specialized);
    call->clearTypeSyntaxes();
    call->setFlag(false);
    return true;
}

bool ProjectGenericModel::processGenericCall(
    ProjectSyntaxUnit& unit,
    ast::AstNode* call,
    const std::string& namespaceName,
    const std::string& ownerName,
    const GenericLookupScope& scope,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (call == nullptr || call->kind() != ast::AstNodeKind::CallExpression) return false;
    for (ast::TypeSyntax* argument : call->typeSyntaxes())
    {
        if (!processTypeSyntax(unit, argument, namespaceName, scope, diagnostics)) return false;
    }

    ast::AstNode* callee = methodCalleeSyntax(call);
    const std::string methodName = methodCalleeName(call);
    // La escritura del receptor, o --si no hay receptor escrito-- la del tipo desde el que se
    // llama, que es como se nombra a un metodo estatico del propio tipo.
    std::string requestedOwner = templateSourceSpelling(namespaceName, ownerName);
    if (callee != nullptr && callee->kind() == ast::AstNodeKind::MemberExpression)
    {
        const std::string written = receiverSourceSpelling(callee->first());
        if (!written.empty()) requestedOwner = written;
    }

    // ID 132. Los argumentos de VALOR de la llamada. Los hijos de un nodo de llamada son sus
    // argumentos; el receptor viaja en el callee, no aqui.
    const std::size_t argumentCount = call->children().size();

    // ID 132, SEGUNDA MITAD -- las que comparten cuenta de parametros y difieren en los TIPOS.
    //
    // Filtrar por cuenta de argumentos separa `Cuenta<T>(T[])` de `Cuenta<T>(T[], int, int)`. Lo
    // que NO puede separar es `F<T>(T)` de `F<T>(T[])`: los dos toman uno, y elegir necesita el
    // TIPO del argumento -- que esta pasada no conoce, porque corre antes del modelo semantico.
    //
    // ANTES SE RENDIA: "duplicate generic template identity", y habia que renombrar.
    // AHORA SE MATERIALIZAN TODAS con el MISMO nombre especializado, y a partir de ese momento son
    // sobrecargas ORDINARIAS -- metodos no genericos con el mismo nombre y listas de parametros
    // distintas, que este compilador resuelve por tipos desde siempre.
    //
    // No se adivina nada: se le pasa el problema a la capa que tiene los datos. Y si dos
    // especializaciones acaban con la MISMA lista de parametros --`F<int>(int)` y `F<T>(int)`--
    // el choque lo detecta la deteccion de metodos duplicados de siempre, que es su sitio.
    // EL SITIO DE LA LLAMADA, en la forma que entiende la escala compartida.
    QualifiedNameSiteContext siteContext = makeQualifiedNameSite(namespaceName, ownerName, scope);
    const QualifiedNameSite& site = siteContext.site;

    const GenericCallResolution resolution = resolveGenericCall(
        unit, site, requestedOwner, methodName, call->typeSyntaxes().size(), argumentCount);
    const std::vector<TemplateState*>& namedAtWinningRung = resolution.named;
    const std::vector<TemplateState*>& shared = resolution.shared;
    if (shared.size() > 1U)
    {
        return materializeSharedOverloads(
            unit, call, callee, methodName, namespaceName, shared, diagnostics);
    }

    TemplateState* state = resolution.unique;
    if (state == nullptr)
    {
        const bool ambiguous = namedAtWinningRung.size() > 1U;
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            call->range(),
            ambiguous
                ? "generic call '" + methodName + "<...>' is ambiguous between " +
                    std::to_string(namedAtWinningRung.size()) +
                    " visible Stage 44 generic method templates"
                : "generic call '" + methodName +
                    "<...>' has no visible Stage 44 generic method template");
        return false;
    }
    if (!genericArityAccepts(state->record, call->typeSyntaxes().size()))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidArgumentCount,
            diagnostics::DiagnosticSeverity::Error,
            call->range(),
            "generic method '" + methodName + "' expects " +
                (trailingPackIndex(state->record).has_value()
                    ? "at least " + std::to_string(*trailingPackIndex(state->record))
                    : std::to_string(state->record.parameters.size())) +
                " type argument(s) but received " +
                std::to_string(call->typeSyntaxes().size()));
        return false;
    }
    return materializeUniqueMethod(unit, call, callee, *state, namespaceName, diagnostics);
}

ProjectGenericModel::TypeResolution ProjectGenericModel::resolveTypeTemplate(
    const ProjectSyntaxUnit& unit,
    std::string_view namespaceName,
    std::string_view writtenName,
    std::size_t arity)
{
    const auto sinFiltro = std::numeric_limits<std::size_t>::max();
    const std::size_t moduleSeparator = writtenName.find("::");
    if (moduleSeparator != std::string_view::npos)
    {
        const std::string_view requestedModule = writtenName.substr(0, moduleSeparator);
        const std::string_view requestedType = writtenName.substr(moduleSeparator + 2U);
        if (requestedModule.empty() || requestedType.empty())
        {
            telemetry_.noteResolutionQuery(0);
            return TypeResolution{TypeResolutionKind::Missing, nullptr};
        }

        const auto bucket = typesByModuleQualified_.find(
            std::string(requestedModule) + "::" + std::string(requestedType));
        if (bucket == typesByModuleQualified_.end())
        {
            telemetry_.noteResolutionQuery(0);
            return TypeResolution{TypeResolutionKind::Missing, nullptr};
        }
        telemetry_.noteResolutionQuery(bucket->second.size());
        TemplateState* exact = nullptr;
        for (TemplateState* state : bucket->second)
        {
            if (arity != sinFiltro && !genericArityAccepts(state->record, arity)) continue;
            if (exact != nullptr) return TypeResolution{TypeResolutionKind::Ambiguous, nullptr};
            exact = state;
        }
        return TypeResolution{
            exact != nullptr ? TypeResolutionKind::Unique : TypeResolutionKind::Missing, exact};
    }

    // NSC305-G02. Bucket by terminal name; the original scan tested every catalog entry for
    // visibility, arity and candidate spelling. The bucket keeps source order, so the first-match
    // and ambiguity semantics are unchanged.
    const std::string qualified = qualifiedName(namespaceName, writtenName);
    const std::string_view terminalName = [&]() -> std::string_view
    {
        const std::size_t dot = writtenName.rfind('.');
        return dot == std::string_view::npos ? writtenName : writtenName.substr(dot + 1U);
    }();
    const auto bucket = typesByVisibleName_.find(std::string(terminalName));
    if (bucket == typesByVisibleName_.end())
    {
        telemetry_.noteResolutionQuery(0);
        return TypeResolution{TypeResolutionKind::Missing, nullptr};
    }
    telemetry_.noteResolutionQuery(bucket->second.size());
    TemplateState* fallback = nullptr;
    for (TemplateState* state : bucket->second)
    {
        if (!templateVisible(unit.projectOrdinal(), state->record)) continue;
        if (arity != sinFiltro && !genericArityAccepts(state->record, arity)) continue;
        const std::string candidate =
            qualifiedName(state->record.namespaceName, state->record.qualifiedTypeName);
        if (writtenName.find('.') != std::string_view::npos)
        {
            if (candidate == writtenName)
            {
                if (fallback != nullptr) return TypeResolution{TypeResolutionKind::Ambiguous, nullptr};
                fallback = state;
            }
            continue;
        }
        if (candidate == qualified) return TypeResolution{TypeResolutionKind::Unique, state};
        if (state->record.name == writtenName)
        {
            if (fallback != nullptr) return TypeResolution{TypeResolutionKind::Ambiguous, nullptr};
            fallback = state;
        }
    }
    return TypeResolution{
        fallback != nullptr ? TypeResolutionKind::Unique : TypeResolutionKind::Missing, fallback};
}

// NSC-LIMIT-007. QUE PLANTILLAS NOMBRA UNA LLAMADA -- con la escala del modelo de nombres, no
// con una propia.
//
// La version anterior tenia tres escalones inventados aqui: exacto, relativo al namespace del que
// llama, y sufijo de segmentos. Eso no es lo que hace este compilador. Dentro de `A.B`, el nombre
// `C` significa `A.B.C` si existe, si no `A.C`, si no lo que traiga un `using`, si no el global --y
// antes que todo eso, un tipo contenedor del sitio: dentro de `Outer`, `Helper` es `Outer.Helper`--.
// Con los tres escalones inventados, `A.C` y `Z.C` eran los DOS sufijos de `.C` y empataban, de
// modo que una llamada perfectamente legal se declaraba ambigua o clonaba la plantilla ajena --con
// su cuerpo, que habla de SU `T`--.
//
// Ahora la escala viene de `qualifiedNameLadder`, que es la MISMA funcion que recorre
// ProjectNameLookup. Lo que cambia entre las dos fases es que se sondea: alli la tabla de tipos,
// aqui la de plantillas. Ver QualifiedNameLadder.h.
std::vector<ProjectGenericModel::TemplateState*> ProjectGenericModel::templatesNamedBy(
    const ProjectSyntaxUnit& unit,
    const QualifiedNameSite& site,
    std::string_view writtenOwner,
    std::string_view methodName,
    std::size_t arity,
    std::size_t argumentCount,
    const std::vector<QualifiedNameRung>* ladder)
{
    const auto bucket = methodsByName_.find(std::string(methodName));
    if (bucket == methodsByName_.end())
    {
        telemetry_.noteResolutionQuery(0);
        return {};
    }
    telemetry_.noteResolutionQuery(bucket->second.size());
    const auto sinFiltro = std::numeric_limits<std::size_t>::max();
    const auto aplicable = [&](const TemplateState& state)
    {
        if (!templateVisible(unit.projectOrdinal(), state.record)) return false;
        if (arity != sinFiltro && !genericArityAccepts(state.record, arity)) return false;
        if (argumentCount != sinFiltro && valueParameterCount(state.syntax) != argumentCount)
            return false;
        return true;
    };

    // EL PRIMER ESCALON QUE PRODUCE ALGO DECIDE, y los de abajo ya no se miran. Es la regla de
    // sombreado: que exista un `Z.C` en otro namespace no puede cambiar lo que `C` significa aqui.
    std::vector<QualifiedNameRung> ownLadder;
    if (ladder == nullptr)
        ownLadder = qualifiedNameLadder(site, writtenOwner);
    const std::vector<QualifiedNameRung>& effectiveLadder = ladder != nullptr ? *ladder : ownLadder;
    for (const QualifiedNameRung& rung : effectiveLadder)
    {
        std::vector<TemplateState*> found;
        for (TemplateState* state : bucket->second)
        {
            if (!aplicable(*state)) continue;
            const std::string owner =
                templateSourceSpelling(state->record.namespaceName, state->record.ownerName);
            for (const std::string& candidate : rung.spellings)
            {
                if (owner == candidate)
                {
                    found.push_back(state);
                    break;
                }
            }
        }
        if (!found.empty()) return found;
    }
    // NS304-T08-F06: a global-namespace template has an empty source owner. The ladder cannot
    // produce an empty rung, so match those templates explicitly.
    if (writtenOwner.empty())
    {
        std::vector<TemplateState*> found;
        for (TemplateState* state : bucket->second)
        {
            if (!aplicable(*state)) continue;
            if (templateSourceSpelling(state->record.namespaceName, state->record.ownerName).empty())
                found.push_back(state);
        }
        if (!found.empty()) return found;
    }
    return {};
}

std::vector<ProjectGenericModel::TemplateState*> ProjectGenericModel::collectMethodTemplatesFromNamed(
    const std::vector<TemplateState*>& named) const
{
    // UN SOLO DUENO. Un escalon puede empatar --dos `using` que traen dos `Caja`, por ejemplo--.
    // Materializar las dos significa clonar y analizar un cuerpo que este codigo no nombra, y ese
    // cuerpo habla de SU `T`. Si el empate cruza duenos la llamada es realmente ambigua y no se
    // materializa nada: `resolveMethodTemplate` da entonces su diagnostico, que es su sitio.
    std::string owner;
    std::string ownerModule;
    for (const TemplateState* state : named)
    {
        const std::string candidate =
            templateSourceSpelling(state->record.namespaceName, state->record.ownerName);
        if (owner.empty())
        {
            owner = candidate;
            ownerModule = state->record.moduleName;
        }
        else if (owner != candidate || ownerModule != state->record.moduleName) return {};
    }
    return named;
}

std::vector<ProjectGenericModel::TemplateState*> ProjectGenericModel::collectMethodTemplates(
    const ProjectSyntaxUnit& unit,
    const QualifiedNameSite& site,
    std::string_view writtenOwner,
    std::string_view methodName,
    std::size_t arity,
    std::size_t argumentCount)
{
    return collectMethodTemplatesFromNamed(
        templatesNamedBy(unit, site, writtenOwner, methodName, arity, argumentCount));
}

ProjectGenericModel::TemplateState* ProjectGenericModel::resolveMethodTemplate(
    const ProjectSyntaxUnit& unit,
    const QualifiedNameSite& site,
    std::string_view writtenOwner,
    std::string_view methodName,
    std::size_t arity,
    std::size_t argumentCount,
    const std::vector<TemplateState*>* named,
    const std::vector<QualifiedNameRung>* ladder)
{
    const auto sinFiltro = std::numeric_limits<std::size_t>::max();

    // CON DUENO: la misma funcion que usa `collectMethodTemplates`, no una copia de sus reglas.
    // Primero exigiendo la cuenta de argumentos y despues sin ella, que es el aflojamiento que
    // este metodo ya tenia. NSC305-G03: el primer escalon puede venir ya calculado (`named`) y la
    // ladder puede venir compartida (`ladder`) para no reconstruirla en cada intento.
    for (const std::size_t cuenta : {argumentCount, sinFiltro})
    {
        const std::vector<TemplateState*> result =
            (cuenta == argumentCount && named != nullptr)
            ? *named
            : templatesNamedBy(unit, site, writtenOwner, methodName, arity, cuenta, ladder);
        if (result.size() == 1) return result.front();
        // Mas de una en el escalon ganador y del mismo nombre: ambigua de verdad. Aflojar la
        // cuenta de argumentos no la desambigua, asi que se sigue al siguiente intento solo
        // cuando no hubo NINGUNA.
        if (!result.empty()) break;
        if (cuenta == sinFiltro) break;
    }

    // SIN DUENO -- y solo aqui, cuando la escala no encontro ninguno.
    //
    // Es lo unico que esta fase tiene para sospechar que el receptor escrito no era un tipo sino un
    // VALOR: `objeto.F<int>()` produce la escritura `objeto`, que no nombra ninguna plantilla
    // porque no nombra ningun tipo. Tomar este camino cuando el dueno SI se resolvio seria
    // deshacer el filtro que se acaba de aplicar, y era justamente lo que hacia antes.
    const auto coincideSinDueno = [&](const TemplateState& state, bool exigirArgumentos)
    {
        if (!templateVisible(unit.projectOrdinal(), state.record)) return false;
        if (arity != sinFiltro && !genericArityAccepts(state.record, arity)) return false;
        if (exigirArgumentos && argumentCount != sinFiltro &&
            valueParameterCount(state.syntax) != argumentCount)
            return false;
        return true;
    };
    const auto unico = [&](bool exigirArgumentos) -> TemplateState*
    {
        const auto bucket = methodsByName_.find(std::string(methodName));
        if (bucket == methodsByName_.end()) return nullptr;
        TemplateState* result = nullptr;
        for (TemplateState* state : bucket->second)
        {
            if (!coincideSinDueno(*state, exigirArgumentos)) continue;
            if (result != nullptr) return nullptr;
            result = state;
        }
        return result;
    };
    if (TemplateState* result = unico(true); result != nullptr) return result;
    return unico(false);
}

// NSC305-G07. Shared steps of the ensure*Specialization pair; see the header.
std::string ProjectGenericModel::computeSpecializationKey(
    const GenericTemplateRecord& record,
    std::span<ast::TypeSyntax* const> arguments,
    std::vector<std::string>& spellings,
    std::vector<std::string>& identities) const
{
    spellings.reserve(arguments.size());
    identities.reserve(arguments.size());
    for (const ast::TypeSyntax* argument : arguments)
    {
        spellings.push_back(typeSpelling(argument));
        identities.push_back(typeSpecializationIdentity(argument));
    }
    std::ostringstream keyBuilder;
    keyBuilder << record.canonicalKey << '<';
    for (std::size_t index = 0; index < spellings.size(); ++index)
    {
        if (index != 0) keyBuilder << ',';
        keyBuilder << identities[index];
    }
    keyBuilder << '>';
    return keyBuilder.str();
}

void ProjectGenericModel::prepareSpecializationSubstitutions(
    TemplateState& state,
    std::span<ast::TypeSyntax* const> arguments,
    std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions)
{
    const std::size_t fixedParameterCount = trailingPackIndex(state.record).value_or(
        state.record.parameters.size());
    for (std::size_t index = 0; index < fixedParameterCount; ++index)
        substitutions.emplace(state.record.parameters[index], arguments[index]);
    activePackSubstitutions_.clear();
    if (const auto pack = trailingPackIndex(state.record); pack.has_value())
    {
        auto& values = activePackSubstitutions_[state.record.parameters[*pack]];
        for (std::size_t index = *pack; index < arguments.size(); ++index)
            values.push_back(arguments[index]);
    }
}

// NSC305-G08. Build a QualifiedNameSite once, owning the vectors its spans point into.
ProjectGenericModel::QualifiedNameSiteContext ProjectGenericModel::makeQualifiedNameSite(
    std::string_view namespaceName,
    std::string_view ownerName,
    const GenericLookupScope& scope) const
{
    QualifiedNameSiteContext context;
    context.namespaceSegments = splitDotted(namespaceName);
    context.enclosingTypeNames = enclosingTypeSpellings(namespaceName, ownerName);
    context.site.namespaceSegments = context.namespaceSegments;
    context.site.enclosingTypeNames = context.enclosingTypeNames;
    context.site.aliases = &scope.aliases.read();
    context.site.importedNamespaces = scope.importedNamespaces.read();
    return context;
}

// NSC305-G08. Resolve a generic call site once: named candidates at the winning rung, the
// shared-overload set, and the unique fallback (exact arity, then relaxed arity).
ProjectGenericModel::GenericCallResolution ProjectGenericModel::resolveGenericCall(
    const ProjectSyntaxUnit& unit,
    const QualifiedNameSite& site,
    std::string_view requestedOwner,
    std::string_view methodName,
    std::size_t arity,
    std::size_t argumentCount)
{
    // NSC305-G03. Build the name ladder once; every arity/argument-count variation reuses it.
    const std::vector<QualifiedNameRung> ladder = qualifiedNameLadder(site, requestedOwner);
    GenericCallResolution resolution;
    resolution.named = templatesNamedBy(
        unit, site, requestedOwner, methodName, arity, argumentCount, &ladder);
    resolution.shared = collectMethodTemplatesFromNamed(resolution.named);
    if (resolution.shared.size() <= 1U)
    {
        resolution.unique = resolveMethodTemplate(
            unit, site, requestedOwner, methodName, arity, argumentCount, nullptr, &ladder);
        if (resolution.unique == nullptr)
            resolution.unique = resolveMethodTemplate(
                unit, site, requestedOwner, methodName,
                std::numeric_limits<std::size_t>::max(), argumentCount, nullptr, &ladder);
    }
    return resolution;
}

// NSC305-G09. Merges a produced specialization clone into its reserved slot (see the header).
void ProjectGenericModel::commitTypeSpecialization(
    TemplateState& state,
    ast::AstNode* clone,
    const std::string& key,
    const std::string& specializedName,
    std::size_t ordinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    clone->setText(specializedName);
    clone->clearGenericParameters();
    // Keep the specialization prefix first because downstream demand/debug readers use it as the
    // discriminator. Source aggregate metadata follows a reserved delimiter: a comma cannot be used
    // because it is valid inside a multi-argument specialization key. Record derives, layout, union
    // and borrowed-view policy belong to the template and must survive monomorphization.
    std::string specializationMetadata = "stage44-specialization:" + key;
    if (!state.syntax->auxiliaryText().empty())
        specializationMetadata += ";nsc-meta:" + state.syntax->auxiliaryText();
    clone->setAuxiliaryText(std::move(specializationMetadata));
    state.parent->addChild(clone);
    specializations_[ordinal].specializedSyntax = clone;
    specializationBySyntax_[clone] = ordinal;
    specializations_[ordinal].state = SpecializationState::Ready;
    registerAssociatedItems(
        clone,
        namespaceAppend(state.record.namespaceName, specializedName),
        &diagnostics);
}

// NSC305-G09. Reserves a slot + ordinal for a type specialization (see the header).
std::size_t ProjectGenericModel::reserveTypeSpecialization(
    const TemplateState& state,
    const std::string& key,
    const std::string& specializedName,
    const std::vector<std::string>& identities)
{
    const std::size_t ordinal = specializations_.size();
    specializationPlanner_.commit(key, ordinal);
    specializations_.push_back(GenericSpecializationRecord{
        state.record.kind,
        state.syntax,
        nullptr,
        state.record.canonicalKey,
        key,
        specializedName,
        identities,
        ordinal,
        state.ordinal,
        nullptr,
        currentInstantiationRange_,
        currentOverloadGroup_,
        InstantiationContext{
            currentInstantiationProjectOrdinal_,
            currentInstantiationNamespace_,
            currentInstantiationRange_,
            currentOverloadGroup_,
            activePackSubstitutions_,
            activeAssociatedProjections_}});
    specializations_.back().state = SpecializationState::Building;
    return ordinal;
}

// NSC305-G09. Executes a reserved type specialization (see the header).
bool ProjectGenericModel::executeTypeSpecialization(
    TemplateState& state,
    std::span<ast::TypeSyntax* const> arguments,
    const std::string& key,
    const std::string& specializedName,
    std::size_t ordinal,
    const SpecializationContext& callerContext,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
    prepareSpecializationSubstitutions(state, arguments, substitutions);
    packMaterializationError_.clear();
    packMaterializationErrorRange_ = {};
    activeAssociatedProjections_.clear();
    associatedProjectionError_.clear();
    associatedProjectionErrorRange_ = {};

    ast::AstNode* clone = cloneNode(
        state.unit->arena(), state.syntax, substitutions, specializedName, state.record.name);
    const std::string packError = packMaterializationError_;
    const SourceRange packErrorRange = packMaterializationErrorRange_;
    const std::string associatedError = associatedProjectionError_;
    const SourceRange associatedErrorRange = associatedProjectionErrorRange_;
    packMaterializationError_ = callerContext.packError;
    packMaterializationErrorRange_ = callerContext.packErrorRange;
    activePackSubstitutions_ = callerContext.packSubstitutions;
    activeAssociatedProjections_ = callerContext.associatedProjections;
    associatedProjectionError_ = callerContext.associatedError;
    associatedProjectionErrorRange_ = callerContext.associatedErrorRange;
    if (!packError.empty() || !associatedError.empty())
    {
        diagnostics.report(
            associatedError.empty()
                ? diagnostics::DiagnosticId::InvalidArgumentCount
                : diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            associatedError.empty() ? packErrorRange : associatedErrorRange,
            associatedError.empty() ? packError : associatedError);
        return false;
    }
    {
        const GenericTelemetry::Scope commit(telemetry_, "generics.rewrite-commit");
        commitTypeSpecialization(state, clone, key, specializedName, ordinal, diagnostics);
    }

    processNode(
        *state.unit, clone, state.record.namespaceName, specializedName,
        GenericLookupScope{}, false, diagnostics);
    // NSC305-C03. Persist the specialized clone (the C3 cache payload) under the specialization key.
    if (templateCache_.enabled())
    {
        const std::string payload = serializeAstNodeTree(clone);
        std::string error;
        (void)templateCache_.store(key, payload, error);
    }
    // processNode may recursively materialize another generic and therefore replace all four
    // current-instantiation fields. They describe a call stack, not global "last call" state:
    // restore the caller before returning so a sibling specialization (notably the next overload
    // candidate) cannot inherit the nested library context.
    currentInstantiationProjectOrdinal_ = callerContext.projectOrdinal;
    currentInstantiationNamespace_ = callerContext.namespaceName;
    currentInstantiationRange_ = callerContext.range;
    currentOverloadGroup_ = callerContext.overloadGroup;
    activePackSubstitutions_ = callerContext.packSubstitutions;
    return true;
}

std::string ProjectGenericModel::ensureTypeSpecialization(
    TemplateState& state,
    std::span<ast::TypeSyntax* const> arguments,
    diagnostics::DiagnosticEngine& diagnostics,
    std::string* outSpecializationKey)
{
    SpecializationContext callerContext;
    callerContext.save(*this);

    std::vector<std::string> spellings;
    std::vector<std::string> identities;
    const std::string key = computeSpecializationKey(
        state.record, arguments, spellings, identities);
    if (const auto found = specializationPlanner_.request(key))
    {
        telemetry_.noteSpecializationHit();
        ++instantiationCacheHitCount_;
        if (outSpecializationKey != nullptr) *outSpecializationKey = key;
        return specializations_[*found].specializedName;
    }

    if (state.unit == nullptr || state.parent == nullptr || state.syntax == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            state.syntax != nullptr ? state.syntax->range() : SourceRange{},
            "Stage 44 generic type template has no materialization parent");
        return {};
    }
    std::string budgetError;
    if (!instantiationBudget_.enter(key, budgetError))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            currentInstantiationRange_,
            budgetError);
        return {};
    }
    SpecializationWaveTimer wave(telemetry_, state.record.canonicalKey);

    std::string specializedName = state.record.name + "__g" + std::to_string(arguments.size());
    for (const std::string& spelling : spellings) specializedName += "__" + sanitize(spelling);
    specializedName += "__h" + stableHashSuffix(key);

    const std::size_t ordinal = reserveTypeSpecialization(state, key, specializedName, identities);

    if (!executeTypeSpecialization(
            state, arguments, key, specializedName, ordinal, callerContext, diagnostics))
    {
        specializationPlanner_.rollback(key);
        specializations_.pop_back();
        currentInstantiationProjectOrdinal_ = callerContext.projectOrdinal;
        currentInstantiationNamespace_ = callerContext.namespaceName;
        currentInstantiationRange_ = callerContext.range;
        currentOverloadGroup_ = callerContext.overloadGroup;
        instantiationBudget_.leave();
        return {};
    }
    instantiationBudget_.leave();
    if (outSpecializationKey != nullptr) *outSpecializationKey = key;
    return specializedName;
}

std::string ProjectGenericModel::ensureMethodSpecialization(
    TemplateState& state,
    std::span<ast::TypeSyntax* const> arguments,
    diagnostics::DiagnosticEngine& diagnostics,
    std::string_view sharedName)
{
    SpecializationContext callerContext;
    callerContext.save(*this);

    std::vector<std::string> spellings;
    std::vector<std::string> identities;
    const std::string key = computeSpecializationKey(
        state.record, arguments, spellings, identities);
    if (const auto found = specializationPlanner_.request(key))
    {
        telemetry_.noteSpecializationHit();
        ++instantiationCacheHitCount_;
        return specializations_[*found].specializedName;
    }

    if (state.unit == nullptr || state.parent == nullptr || state.syntax == nullptr)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            state.syntax != nullptr ? state.syntax->range() : SourceRange{},
            "Stage 44 generic method template has no materialization parent");
        return {};
    }
    std::string budgetError;
    if (!instantiationBudget_.enter(key, budgetError))
    {
        diagnostics.report(
            diagnostics::DiagnosticId::InvalidCall,
            diagnostics::DiagnosticSeverity::Error,
            currentInstantiationRange_,
            budgetError);
        return {};
    }
    SpecializationWaveTimer wave(telemetry_, state.record.canonicalKey);

    // ID 132, segunda mitad. `sharedName` no esta vacio cuando VARIAS plantillas comparten nombre
    // y aridad generica: todas se especializan con EL MISMO nombre, y a partir de ahi dejan de ser
    // un problema de genericos y pasan a ser SOBRECARGAS ORDINARIAS, que este compilador ya
    // resuelve por lista de parametros desde siempre.
    //
    // Es la unica forma honesta de hacerlo en esta pasada: elegir aqui exigiria los TIPOS de los
    // argumentos, y el modelo semantico --el unico que los conoce-- corre despues. En vez de
    // adivinar, se materializan todas y se deja elegir a quien sabe.
    //
    // La CLAVE sigue llevando la identidad de cada plantilla (con su `#N`), asi que la cache
    // sigue distinguiendolas; lo unico compartido es el nombre emitido.
    std::string specializedName;
    if (!sharedName.empty())
    {
        specializedName = std::string(sharedName);
    }
    else
    {
        specializedName = state.record.name + "__g" + std::to_string(arguments.size());
        for (const std::string& spelling : spellings) specializedName += "__" + sanitize(spelling);
        specializedName += "__h" + stableHashSuffix(key);
    }

    const std::size_t ordinal = specializations_.size();
    specializationPlanner_.commit(key, ordinal);
    specializations_.push_back(GenericSpecializationRecord{
        GenericTemplateKind::Method,
        state.syntax,
        nullptr,
        state.record.canonicalKey,
        key,
        specializedName,
        identities,
        ordinal,
        state.ordinal,
        nullptr,
        currentInstantiationRange_,
        currentOverloadGroup_,
        InstantiationContext{
            currentInstantiationProjectOrdinal_,
            currentInstantiationNamespace_,
            currentInstantiationRange_,
            currentOverloadGroup_,
            activePackSubstitutions_,
            activeAssociatedProjections_}});
    specializations_.back().state = SpecializationState::Building;

    std::unordered_map<std::string, const ast::TypeSyntax*> substitutions;
    prepareSpecializationSubstitutions(state, arguments, substitutions);
    packMaterializationError_.clear();
    packMaterializationErrorRange_ = {};
    activeAssociatedProjections_.clear();
    associatedProjectionError_.clear();
    associatedProjectionErrorRange_ = {};

    ast::AstNode* clone = cloneNode(state.unit->arena(), state.syntax, substitutions, "", "");
    const std::string packError = packMaterializationError_;
    const SourceRange packErrorRange = packMaterializationErrorRange_;
    const std::string associatedError = associatedProjectionError_;
    const SourceRange associatedErrorRange = associatedProjectionErrorRange_;
    packMaterializationError_ = callerContext.packError;
    packMaterializationErrorRange_ = callerContext.packErrorRange;
    activePackSubstitutions_ = callerContext.packSubstitutions;
    activeAssociatedProjections_ = callerContext.associatedProjections;
    associatedProjectionError_ = callerContext.associatedError;
    associatedProjectionErrorRange_ = callerContext.associatedErrorRange;
    if (!packError.empty() || !associatedError.empty())
    {
        specializationPlanner_.rollback(key);
        specializations_.pop_back();
        diagnostics.report(
            associatedError.empty()
                ? diagnostics::DiagnosticId::InvalidArgumentCount
                : diagnostics::DiagnosticId::InvalidType,
            diagnostics::DiagnosticSeverity::Error,
            associatedError.empty() ? packErrorRange : associatedErrorRange,
            associatedError.empty() ? packError : associatedError);
        currentInstantiationProjectOrdinal_ = callerContext.projectOrdinal;
        currentInstantiationNamespace_ = callerContext.namespaceName;
        currentInstantiationRange_ = callerContext.range;
        currentOverloadGroup_ = callerContext.overloadGroup;
        instantiationBudget_.leave();
        return {};
    }
    {
        const GenericTelemetry::Scope commit(telemetry_, "generics.rewrite-commit");
        clone->setText(specializedName);
        clone->clearGenericParameters();
        std::string specializationMetadata = "stage44-specialization:" + key;
        if (!state.syntax->auxiliaryText().empty())
            specializationMetadata += ";nsc-meta:" + state.syntax->auxiliaryText();
        clone->setAuxiliaryText(std::move(specializationMetadata));
        state.parent->addChild(clone);
        specializations_[ordinal].specializedSyntax = clone;
        specializationBySyntax_[clone] = ordinal;
        specializations_[ordinal].state = SpecializationState::Ready;
    }
    processNode(
        *state.unit, clone, state.record.namespaceName, state.record.ownerName,
        GenericLookupScope{}, false, diagnostics);
    currentInstantiationProjectOrdinal_ = callerContext.projectOrdinal;
    currentInstantiationNamespace_ = callerContext.namespaceName;
    currentInstantiationRange_ = callerContext.range;
    currentOverloadGroup_ = callerContext.overloadGroup;
    activePackSubstitutions_ = callerContext.packSubstitutions;
    instantiationBudget_.leave();
    return specializedName;
}

std::vector<ast::TypeSyntax*> ProjectGenericModel::cloneExpandedTypeArgument(
    ast::AstArena& arena,
    const ast::TypeSyntax* source,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions)
{
    if (source == nullptr) return {nullptr};
    if (!source->isPackExpansion()) return {cloneType(arena, source, substitutions)};

    std::vector<std::string> packNames;
    // NSC305-G06. Iterative walk of the type-argument tree (was a std::function lambda recursion).
    std::vector<const ast::TypeSyntax*> collectStack;
    collectStack.push_back(source);
    while (!collectStack.empty())
    {
        const ast::TypeSyntax* syntax = collectStack.back();
        collectStack.pop_back();
        if (syntax == nullptr) continue;
        if (syntax->typeArguments().empty() &&
            activePackSubstitutions_.contains(syntax->baseName()) &&
            std::find(packNames.begin(), packNames.end(), syntax->baseName()) == packNames.end())
            packNames.push_back(syntax->baseName());
        for (const ast::TypeSyntax* argument : syntax->typeArguments())
            collectStack.push_back(argument);
    }
    if (packNames.empty())
    {
        if (packMaterializationError_.empty())
        {
            packMaterializationError_ =
                "generic pack expansion does not name an active pack";
            packMaterializationErrorRange_ = source->range();
        }
        return {};
    }

    std::vector<std::size_t> lengths;
    lengths.reserve(packNames.size());
    for (const std::string& name : packNames)
        lengths.push_back(activePackSubstitutions_.at(name).size());
    std::string validationError;
    if (!language::validatePackExpansion(
            lengths, DefaultMaximumPackExpansionCount, validationError))
    {
        if (packMaterializationError_.empty())
        {
            packMaterializationError_ = validationError;
            packMaterializationErrorRange_ = source->range();
        }
        return {};
    }

    std::vector<ast::TypeSyntax*> result;
    result.reserve(lengths.front());
    for (std::size_t index = 0; index < lengths.front(); ++index)
    {
        std::unordered_map<std::string, const ast::TypeSyntax*> itemSubstitutions = substitutions;
        for (const std::string& name : packNames)
            itemSubstitutions[name] = activePackSubstitutions_.at(name)[index];
        ast::TypeSyntax* item = cloneType(arena, source, itemSubstitutions);
        if (item != nullptr) item->setPackExpansion(false);
        result.push_back(item);
    }
    return result;
}

namespace
{
// NSC305-G06. RAII depth guard for the mutually-recursive cloneType/cloneNode pair.
constexpr std::size_t kMaximumCloneDepth = 1024;
struct CloneDepthGuard final
{
    std::size_t& depth;
    explicit CloneDepthGuard(std::size_t& value) : depth(value) { ++depth; }
    ~CloneDepthGuard() { --depth; }
};
}  // namespace

// NSC305-G06. The non-substitution branch of cloneType (see the header).
// NSC305-G06. The shallow (non-recursive) part of the generic type clone (see the header).
ast::TypeSyntax* ProjectGenericModel::cloneTypeGenericShallow(
    ast::AstArena& arena,
    const ast::TypeSyntax* source)
{
    ast::TypeSyntax* result =
        arena.make<ast::TypeSyntax>(source->range(), source->baseTokenKind(), source->baseName());
    result->setFunction(source->isFunction());
    result->setFunctionCarrier(source->functionCarrier());
    result->setFunctionCallingConvention(source->functionCallingConvention());
    result->setFunctionVariadic(source->functionIsVariadic());
    result->setFunctionUnsafe(source->functionIsUnsafe());
    result->setFunctionNoHeap(source->functionIsNoHeap());
    result->setFunctionAsync(source->functionIsAsync());
    result->setFunctionNullable(source->functionIsNullable());
    result->setFunctionLifetimeName(source->functionLifetimeName());
    // Stage 52 fix: carry forward the origin tags the SOURCE node already carries.
    if (source->originProjectOrdinal().has_value())
    {
        result->setOriginProjectOrdinal(source->originProjectOrdinal().value());
    }
    if (source->originNamespace().has_value())
    {
        result->setOriginNamespace(source->originNamespace().value());
    }
    if (const auto origin = resolvedTypeOriginBySyntax_.find(source);
        origin != resolvedTypeOriginBySyntax_.end())
    {
        resolvedTypeOriginBySyntax_[result] = origin->second;
    }
    return result;
}

// NSC305-G06. Normalizes an associated type projection (`T.Item`); see the header.
ast::TypeSyntax* ProjectGenericModel::cloneProjectionType(
    ast::AstArena& arena,
    const ast::TypeSyntax* source,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    std::size_t modifierLimit)
{
    const std::size_t projectionSeparator = source->baseName().find('.');
    if (projectionSeparator == std::string::npos) return nullptr;
    const std::string root = source->baseName().substr(0, projectionSeparator);
    const auto substitution = substitutions.find(root);
    if (substitution == substitutions.end()) return nullptr;

    const ast::TypeSyntax* current = substitution->second;
    std::size_t begin = projectionSeparator + 1U;
    while (begin < source->baseName().size())
    {
        const std::size_t end = source->baseName().find('.', begin);
        const std::string item = source->baseName().substr(
            begin, end == std::string::npos ? std::string::npos : end - begin);
        const std::string owner = typeSpelling(current);
        std::vector<std::string> candidates{owner + "." + item};
        if (current != nullptr)
        {
            if (const auto identity = resolvedTypeIdentityBySyntax_.find(current);
                identity != resolvedTypeIdentityBySyntax_.end())
            {
                const std::size_t moduleSeparator = identity->second.find("::");
                if (moduleSeparator != std::string::npos)
                    candidates.push_back(identity->second.substr(moduleSeparator + 2U) +
                        "." + item);
            }
        }
        if (current != nullptr && current->originNamespace().has_value() &&
            !current->originNamespace()->empty())
            candidates.push_back(namespaceAppend(
                *current->originNamespace(), owner) + "." + item);
        if (!currentInstantiationNamespace_.empty())
            candidates.push_back(namespaceAppend(
                currentInstantiationNamespace_, owner) + "." + item);

        const ast::TypeSyntax* definition = nullptr;
        std::string selectedKey;
        for (const std::string& candidate : candidates)
        {
            if (const auto found = associatedTypes_.find(candidate);
                found != associatedTypes_.end())
            {
                definition = found->second;
                selectedKey = candidate;
                break;
            }
        }
        if (definition == nullptr)
        {
            if (associatedProjectionError_.empty())
            {
                associatedProjectionError_ =
                    "cannot normalize associated type projection '" +
                    source->baseName() + "' for concrete type '" + owner +
                    "': no associated type definition is available";
                associatedProjectionErrorRange_ = source->range();
            }
            return arena.make<ast::TypeSyntax>(
                source->range(), lexer::TokenKind::KwInt, "int");
        }
        if (!activeAssociatedProjections_.insert(selectedKey).second)
        {
            if (associatedProjectionError_.empty())
            {
                associatedProjectionError_ =
                    "associated type normalization cycle reaches '" + selectedKey + "'";
                associatedProjectionErrorRange_ = source->range();
            }
            return arena.make<ast::TypeSyntax>(
                source->range(), lexer::TokenKind::KwInt, "int");
        }
        current = cloneType(arena, definition, substitutions);
        activeAssociatedProjections_.erase(selectedKey);
        if (end == std::string::npos) break;
        begin = end + 1U;
    }

    ast::TypeSyntax* result = cloneType(arena, current, {});
    const std::size_t modifierCount = std::min(source->modifiers().size(), modifierLimit);
    for (std::size_t index = 0; index < modifierCount; ++index)
    {
        const ast::TypeModifierSyntax& modifier = source->modifiers()[index];
        result->addModifier(ast::TypeModifierSyntax{
            modifier.kind,
            cloneNode(arena, modifier.arrayCount, substitutions, "", ""),
            modifier.range,
            modifier.arrayRank});
    }
    result->setOwnershipQualifier(source->ownershipQualifier());
    result->setDynamicTrait(source->isDynamicTrait());
    result->setOpaqueResult(source->isOpaqueResult());
    result->setOpaqueResultTraitName(source->opaqueResultTraitName());
    result->setExplicitTraitImplementation(source->isExplicitTraitImplementation());
    result->setPointeeConst(source->isPointeeConst());
    result->setPackExpansion(source->isPackExpansion());
    result->setRange(source->range());
    return result;
}

ast::TypeSyntax* ProjectGenericModel::cloneTypeGeneric(
    ast::AstArena& arena,
    const ast::TypeSyntax* source,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    std::size_t modifierLimit)
{
    ast::TypeSyntax* result = cloneTypeGenericShallow(arena, source);
    if (source->constExpression() != nullptr)
    {
        result->setConstExpression(cloneNode(
            arena, source->constExpression(), substitutions, "", ""));
    }
    // NSC305-G06. Iteratively clone the return/error/argument types with an explicit stack.
    // Simple generic types are shallow-cloned here and their sub-types pushed; projection, pack,
    // substitution, tuple and pack-expansion types delegate back to cloneType (bounded/rare).
    const auto isSpecialType = [&](const ast::TypeSyntax* syntax)
    {
        if (syntax == nullptr) return false;
        const std::size_t dot = syntax->baseName().find('.');
        if (dot != std::string::npos &&
            substitutions.contains(syntax->baseName().substr(0, dot)))
            return true;
        if (syntax->baseName() == "packtype" && syntax->typeArguments().size() == 2U)
            return true;
        if (syntax->typeArguments().empty() && substitutions.contains(syntax->baseName()))
            return true;
        return syntax->isTuple();
    };
    struct TypeWork
    {
        const ast::TypeSyntax* source;
        ast::TypeSyntax* parent;
        std::uint8_t slot;  // 0=return, 1=error, 2=argument
        std::string tupleName;
    };
    std::vector<TypeWork> work;
    if (source->isTuple())
    {
        for (std::size_t index = source->typeArguments().size(); index > 0; --index)
        {
            const std::size_t i = index - 1U;
            const std::string name = i < source->tupleElementNames().size()
                ? source->tupleElementNames()[i] : std::string{};
            work.push_back(TypeWork{source->typeArguments()[i], result, 2, name});
        }
    }
    else
    {
        for (auto it = source->typeArguments().rbegin(); it != source->typeArguments().rend(); ++it)
            work.push_back(TypeWork{*it, result, 2, {}});
    }
    if (source->functionErrorType() != nullptr)
        work.push_back(TypeWork{source->functionErrorType(), result, 1, {}});
    if (source->functionReturnType() != nullptr)
        work.push_back(TypeWork{source->functionReturnType(), result, 0, {}});
    while (!work.empty())
    {
        TypeWork item = work.back();
        work.pop_back();
        ast::TypeSyntax* child = nullptr;
        if (item.source != nullptr)
        {
            if (item.slot == 2 && !item.parent->isTuple() && item.source->isPackExpansion())
            {
                for (ast::TypeSyntax* expanded :
                     cloneExpandedTypeArgument(arena, item.source, substitutions))
                    item.parent->addTypeArgument(expanded);
                continue;
            }
            if (isSpecialType(item.source))
            {
                child = cloneType(arena, item.source, substitutions);
            }
            else
            {
                child = cloneTypeGenericShallow(arena, item.source);
                if (item.source->constExpression() != nullptr)
                {
                    child->setConstExpression(cloneNode(
                        arena, item.source->constExpression(), substitutions, "", ""));
                }
                // simple modifiers (no substitution identity handling)
                const std::size_t count =
                    std::min(item.source->modifiers().size(), modifierLimit);
                for (std::size_t i = 0; i < count; ++i)
                {
                    const ast::TypeModifierSyntax& modifier = item.source->modifiers()[i];
                    child->addModifier(ast::TypeModifierSyntax{
                        modifier.kind,
                        cloneNode(arena, modifier.arrayCount, substitutions, "", ""),
                        modifier.range,
                        modifier.arrayRank});
                }
                for (auto it = item.source->typeArguments().rbegin();
                     it != item.source->typeArguments().rend(); ++it)
                    work.push_back(TypeWork{*it, child, 2, {}});
                if (item.source->functionErrorType() != nullptr)
                    work.push_back(TypeWork{item.source->functionErrorType(), child, 1, {}});
                if (item.source->functionReturnType() != nullptr)
                    work.push_back(TypeWork{item.source->functionReturnType(), child, 0, {}});
            }
        }
        if (child == nullptr) continue;
        switch (item.slot)
        {
            case 0: item.parent->setFunctionReturnType(child); break;
            case 1: item.parent->setFunctionErrorType(child); break;
            case 2:
                if (item.parent->isTuple())
                    item.parent->addTupleElement(child, item.tupleName);
                else
                    item.parent->addTypeArgument(child);
                break;
        }
    }
    return result;
}

ast::TypeSyntax* ProjectGenericModel::cloneType(
    ast::AstArena& arena,
    const ast::TypeSyntax* source,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    std::size_t modifierLimit)
{
    if (source == nullptr) return nullptr;
    CloneDepthGuard depthGuard(cloneDepth_);
    if (cloneDepth_ > kMaximumCloneDepth)
    {
        if (packMaterializationError_.empty())
        {
            packMaterializationError_ =
                "generic materialization exceeds the maximum nesting depth";
            packMaterializationErrorRange_ = source->range();
        }
        return arena.make<ast::TypeSyntax>(
            source->range(), lexer::TokenKind::KwInt, "int");
    }
    telemetry_.noteTypesCloned();

    // T05-F03. Normalize `T.Item` only when the leftmost component is an active generic
    // substitution. Ordinary dotted namespace/type names remain untouched. Chained projections
    // are consumed one component at a time (NSC305-G06: extracted into cloneProjectionType).
    if (ast::TypeSyntax* projection = cloneProjectionType(
            arena, source, substitutions, modifierLimit))
        return projection;

    if (source->baseName() == "packtype" && source->typeArguments().size() == 2U)
    {
        const ast::TypeSyntax* packSyntax = source->typeArguments()[0];
        const auto pack = packSyntax != nullptr && packSyntax->typeArguments().empty()
            ? activePackSubstitutions_.find(packSyntax->baseName())
            : activePackSubstitutions_.end();
        if (pack != activePackSubstitutions_.end())
        {
            std::string indexType;
            std::string indexValue;
            std::size_t index = 0;
            const ast::TypeSyntax* indexSyntax = source->typeArguments()[1];
            const bool decodedIndex = decodeConstGenericArgument(
                indexSyntax, indexType, indexValue);
            const auto parsedIndex = decodedIndex && indexValue.find('-') == std::string::npos
                ? std::from_chars(
                    indexValue.data(), indexValue.data() + indexValue.size(), index)
                : std::from_chars_result{};
            const bool validIndex = decodedIndex && indexValue.find('-') == std::string::npos &&
                parsedIndex.ec == std::errc{} &&
                parsedIndex.ptr == indexValue.data() + indexValue.size();
            if (!validIndex || index >= pack->second.size())
            {
                if (packMaterializationError_.empty())
                {
                    packMaterializationError_ = validIndex
                        ? "generic pack type index " + std::to_string(index) +
                            " is outside a pack of length " +
                            std::to_string(pack->second.size())
                        : "generic pack type index must be a non-negative compile-time integer";
                    packMaterializationErrorRange_ = source->range();
                }
                return arena.make<ast::TypeSyntax>(
                    source->range(), lexer::TokenKind::KwInt, "int");
            }
            ast::TypeSyntax* selected = cloneType(arena, pack->second[index], substitutions);
            const std::size_t modifierCount = std::min(source->modifiers().size(), modifierLimit);
            for (std::size_t modifierIndex = 0; modifierIndex < modifierCount; ++modifierIndex)
            {
                const ast::TypeModifierSyntax& modifier = source->modifiers()[modifierIndex];
                selected->addModifier(ast::TypeModifierSyntax{
                    modifier.kind,
                    cloneNode(arena, modifier.arrayCount, substitutions, "", ""),
                    modifier.range,
                    modifier.arrayRank});
            }
            selected->setOwnershipQualifier(source->ownershipQualifier());
            selected->setLifetimeName(source->lifetimeName());
            selected->setDynamicTrait(source->isDynamicTrait());
            selected->setOpaqueResult(source->isOpaqueResult());
            selected->setOpaqueResultTraitName(source->opaqueResultTraitName());
            selected->setExplicitTraitImplementation(
                source->isExplicitTraitImplementation());
            selected->setPointeeConst(source->isPointeeConst());
            selected->setRange(source->range());
            return selected;
        }
    }

    ast::TypeSyntax* result = nullptr;
    bool substituted = false;
    const auto substitution = substitutions.find(source->baseName());
    if (substitution != substitutions.end() && source->typeArguments().empty())
    {
        substituted = true;
        result = cloneType(arena, substitution->second, {});
        // Stage 52 fix: `result` now names a type ARGUMENT the caller supplied at the
        // instantiation site (e.g. the "MyOwnClass" in "List<MyOwnClass>"), not part of the
        // template's own source text -- so it must resolve using the REQUESTING project's
        // visibility, not the template's defining project's (see TypeSyntax::
        // originProjectOrdinal and the currentInstantiationProjectOrdinal_/
        // currentInstantiationNamespace_ fields for the full explanation). A builtin keyword
        // (int, bool, ...) needs neither: it never goes through name resolution at all.
        // Guard against a NESTED substitution re-tagging an argument that a prior, OUTER
        // substitution already tagged correctly. This happens for e.g. List<T>'s own internal
        // use of Predicate<T> (List.ns): specializing List<Payload> substitutes Predicate<T>'s
        // "T" with the SAME (already AppB-tagged) "Stage52LibTest.Payload" node the outer List<T>
        // substitution produced, but that inner Predicate<T>->Predicate<Payload> specialization
        // request itself runs with the LIBRARY's own unit/namespace as ambient context (it is
        // discovered while walking List<Payload>'s cloned body, not from AppB's own source) --
        // so blindly re-tagging here would silently overwrite the correct origin with the
        // library's, right back into the original bug. Once tagged, leave it tagged.
        preserveOrAttachInstantiationOrigin(result);
    }
    else
    {
        result = cloneTypeGeneric(arena, source, substitutions, modifierLimit);
    }

    const std::size_t modifierCount = std::min(source->modifiers().size(), modifierLimit);
    for (std::size_t modifierIndex = 0; modifierIndex < modifierCount; ++modifierIndex)
    {
        const ast::TypeModifierSyntax& modifier = source->modifiers()[modifierIndex];
        result->addModifier(ast::TypeModifierSyntax{
            modifier.kind,
            cloneNode(arena, modifier.arrayCount, substitutions, "", ""),
            modifier.range,
            modifier.arrayRank});
        // In the substitution branch `result` already carries the argument's canonical identity
        // (including modifiers written on that argument). Modifiers written on the template
        // parameter occurrence are outer modifiers and must be appended as well: substituting
        // Foo for T in T[] identifies Foo[], not Foo.
        if (substituted)
        {
            if (auto identity = resolvedTypeIdentityBySyntax_.find(result);
                identity != resolvedTypeIdentityBySyntax_.end())
            {
                if (modifier.kind == ast::TypeModifierSyntaxKind::Pointer)
                    identity->second.push_back('*');
                else if (modifier.kind == ast::TypeModifierSyntaxKind::DynamicArray)
                    identity->second += "[]";
                else if (modifier.kind == ast::TypeModifierSyntaxKind::RangeArray)
                    identity->second += '[' + std::string(
                        modifier.arrayRank > 0 ? modifier.arrayRank - 1 : 0, ',') + ']';
                else if (modifier.kind == ast::TypeModifierSyntaxKind::FlexibleArray)
                    identity->second += "[*]";
                else
                    identity->second += "[fixed]";
            }
        }
    }
    // Stage 52 fix: an ownership qualifier (owned/borrowed) belongs to THIS occurrence of the
    // type -- e.g. the "owned" in a parameter declared "owned T" -- not to whatever concrete
    // type argument T is substituted with. Previously this was dropped whenever the
    // substitution branch above ran (source->baseName() == the generic parameter name), so a
    // generic method like "void Add(owned T value)" would monomorphize to an UNQUALIFIED
    // parameter once T was closed over a reference type, silently losing the "owned"
    // requirement and rejecting every legitimate move(...) call site with "ownership-transfer
    // argument requires an owned parameter". Carry it over unconditionally, exactly like the
    // pointer/array modifiers just above.
    result->setOwnershipQualifier(source->ownershipQualifier());
    result->setLifetimeName(source->lifetimeName());
    result->setDynamicTrait(source->isDynamicTrait());
    result->setOpaqueResult(source->isOpaqueResult());
    result->setOpaqueResultTraitName(source->opaqueResultTraitName());
    result->setExplicitTraitImplementation(source->isExplicitTraitImplementation());
    result->setPointeeConst(source->isPointeeConst());
    result->setPackExpansion(source->isPackExpansion());
    // Stage 52 fix: only stamp the TEMPLATE's own range for the non-substituted branch (the
    // clone still represents source text written at that template position, e.g. `T[]`'s own
    // array-modifier range). For a substituted leaf, `result` already carries the real argument's
    // own range from the inner cloneType call a few lines up -- overwriting it here with the
    // template parameter's range (e.g. Delegates.ns's own "T") is purely a misleading-diagnostic-
    // location bug: it does not affect name resolution (that is namespace/project-ordinal driven,
    // fixed separately above), but it did make an "unknown type name" error for an argument
    // written in a consuming project point at the template's own file instead.
    if (!substituted)
    {
        result->setRange(source->range());
        if (const auto identity = resolvedTypeIdentityBySyntax_.find(source);
            identity != resolvedTypeIdentityBySyntax_.end())
        {
            resolvedTypeIdentityBySyntax_[result] = identity->second;
        }
    }
    return result;
}

void ProjectGenericModel::preserveOrAttachInstantiationOrigin(ast::TypeSyntax* syntax) const
{
    if (syntax == nullptr || lexer::isBuiltinTypeKeyword(syntax->baseTokenKind()) ||
        syntax->originProjectOrdinal().has_value())
    {
        return;
    }

    // This node names a substituted argument, not a type written in the template that now owns
    // the clone. Prefer the declaration selected while processing the request site: the project
    // that writes Dictionary<string, Texture2D> may be a third project, distinct from both the
    // Dictionary template and the project that declares Texture2D. If the early pass could not
    // select a declaration, retain the historical request-site fallback. Keep the spelling
    // untouched either way: origin is lookup context, never specialization identity.
    if (const auto origin = resolvedTypeOriginBySyntax_.find(syntax);
        origin != resolvedTypeOriginBySyntax_.end())
    {
        syntax->setOriginProjectOrdinal(origin->second.projectOrdinal);
        syntax->setOriginNamespace(origin->second.namespaceName);
    }
    else
    {
        syntax->setOriginProjectOrdinal(currentInstantiationProjectOrdinal_);
        syntax->setOriginNamespace(currentInstantiationNamespace_);
    }
}

// NSC305-G06. Folds a const value-pack operation. Returns nullptr when the node is not a pack
// fold, so cloneNode falls through to the generic clone path. (Extracted from cloneNode.)
ast::AstNode* ProjectGenericModel::foldValuePack(
    ast::AstArena& arena,
    const ast::AstNode* source,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    std::string_view rewrittenContainingTypeName,
    std::string_view originalContainingTypeName)
{
    if (source->kind() != ast::AstNodeKind::CallExpression ||
        source->first() == nullptr ||
        source->first()->kind() != ast::AstNodeKind::IdentifierExpression ||
        source->children().empty() || source->children().front() == nullptr ||
        source->children().front()->kind() != ast::AstNodeKind::IdentifierExpression)
    {
        return nullptr;
    }
    const std::string& operation = source->first()->text();
    const auto pack = activePackSubstitutions_.find(source->children().front()->text());
    if (pack == activePackSubstitutions_.end()) return nullptr;
    if (operation == "packcount" && source->children().size() == 1U)
    {
        ast::AstNode* literal = arena.make<ast::AstNode>(
            ast::AstNodeKind::IntegerLiteralExpression, source->range());
        literal->setTokenKind(lexer::TokenKind::IntegerLiteral);
        literal->setIntegerValue(pack->second.size());
        literal->setText(std::to_string(pack->second.size()));
        return literal;
    }

    if (operation == "packat" && source->children().size() == 2U)
    {
        ast::AstNode* clonedIndex = cloneNode(
            arena, source->children()[1], substitutions,
            rewrittenContainingTypeName, originalContainingTypeName);
        const ProjectIntegerEvaluator evaluator(nullptr);
        const std::optional<ProjectIntegerValue> evaluated = evaluator.evaluate(clonedIndex);
        const bool validIndex = evaluated.has_value() && evaluated->asSigned() >= 0;
        const std::size_t index = validIndex
            ? static_cast<std::size_t>(evaluated->bits) : 0U;
        if (validIndex && index < pack->second.size())
        {
            std::string typeKey;
            std::string value;
            if (decodeConstGenericArgument(pack->second[index], typeKey, value))
                if (ast::AstNode* selected = constArgumentExpression(
                        arena, typeKey, value, source->range()))
                    return selected;
        }
        if (packMaterializationError_.empty())
        {
            packMaterializationError_ = !validIndex
                ? "generic value-pack index must be a non-negative compile-time integer"
                : index >= pack->second.size()
                    ? "generic value-pack index " + std::to_string(index) +
                        " is outside a pack of length " +
                        std::to_string(pack->second.size())
                    : "packat requires a const value pack";
            packMaterializationErrorRange_ = source->range();
        }
        return constArgumentExpression(
            arena, "builtin:int", "0", source->range());
    }

    const bool arithmeticFold = operation == "packsum" || operation == "packproduct";
    const bool booleanFold = operation == "packall" || operation == "packany";
    if ((arithmeticFold || booleanFold) && source->children().size() == 1U)
    {
        const lexer::TokenKind foldOperator = operation == "packsum"
            ? lexer::TokenKind::Plus
            : operation == "packproduct"
                ? lexer::TokenKind::Star
                : operation == "packall"
                    ? lexer::TokenKind::AmpAmp : lexer::TokenKind::PipePipe;
        const std::string identity = operation == "packproduct" || operation == "packall"
            ? "1" : "0";
        ast::AstNode* folded = constArgumentExpression(
            arena,
            booleanFold ? "builtin:bool" : "builtin:int",
            booleanFold ? (operation == "packall" ? "true" : "false") : identity,
            source->range());
        bool valid = true;
        for (const ast::TypeSyntax* item : pack->second)
        {
            std::string typeKey;
            std::string value;
            if (!decodeConstGenericArgument(item, typeKey, value) ||
                (booleanFold && typeKey != "builtin:bool") ||
                (arithmeticFold && !ProjectIntegerEvaluator::isIntegerType(typeKey)))
            {
                valid = false;
                break;
            }
            ast::AstNode* element = constArgumentExpression(
                arena, typeKey, value, source->range());
            if (element == nullptr)
            {
                valid = false;
                break;
            }
            ast::AstNode* combined = arena.make<ast::AstNode>(
                ast::AstNodeKind::BinaryExpression, source->range());
            combined->setTokenKind(foldOperator);
            combined->setFirst(folded);
            combined->setSecond(element);
            folded = combined;
        }
        if (valid) return folded;
        if (packMaterializationError_.empty())
        {
            packMaterializationError_ = booleanFold
                ? operation + " requires a bool const value pack"
                : operation + " requires an integral const value pack";
            packMaterializationErrorRange_ = source->range();
        }
        return folded;
    }
    return nullptr;
}

// NSC305-G06. Copies a node's direct scalar fields and modifiers (non-recursive).
ast::AstNode* ProjectGenericModel::cloneNodeCopyFields(
    ast::AstArena& arena,
    const ast::AstNode* source,
    ast::AstNodeKind clonedKind)
{
    ast::AstNode* result = arena.make<ast::AstNode>(clonedKind, source->range());
    result->setText(source->text());
    result->setAuxiliaryText(source->auxiliaryText());
    result->setArgumentName(source->argumentName());
    result->setTokenKind(source->tokenKind());
    result->setFlag(source->flag());
    result->setReference(source->isReference());
    result->setLifetimeName(source->lifetimeName());
    result->setVariadic(source->isVariadic());
    result->setUnsafe(source->isUnsafe());
    result->setFloatingPointMode(source->floatingPointMode());
    result->setFfiUnwindPolicy(source->ffiUnwindPolicy());
    result->setIntegerValue(source->integerValue());
    result->setFloatingValue(source->floatingValue());
    // Modifiers are semantic template state, not decoration.
    for (const lexer::TokenKind modifier : source->modifiers()) result->addModifier(modifier);
    return result;
}

// NSC305-G06. The generic (non-special-case) clone of a node (see the header).
ast::AstNode* ProjectGenericModel::cloneNodeGeneric(
    ast::AstArena& arena,
    const ast::AstNode* source,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    std::string_view rewrittenContainingTypeName,
    std::string_view originalContainingTypeName)
{
    const bool layoutQueryOfSubstitutedParameter =
        (source->kind() == ast::AstNodeKind::SizeofExpression ||
         source->kind() == ast::AstNodeKind::AlignofExpression) &&
        source->first() != nullptr &&
        source->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
        substitutions.contains(source->first()->text());
    ast::AstNodeKind clonedKind = source->kind();
    if (layoutQueryOfSubstitutedParameter)
    {
        clonedKind = source->kind() == ast::AstNodeKind::SizeofExpression
            ? ast::AstNodeKind::SizeofTypeExpression
            : ast::AstNodeKind::AlignofTypeExpression;
    }
    ast::AstNode* result = cloneNodeCopyFields(arena, source, clonedKind);
    // A scalar `new T` inside a generic template allocates one raw storage slot whose element
    // type is T. Preserve the source-level intent before substituting its type.
    if (source->kind() == ast::AstNodeKind::NewExpression && !source->flag() &&
        source->typeSyntax() != nullptr && source->typeSyntax()->typeArguments().empty() &&
        substitutions.contains(source->typeSyntax()->baseName()))
    {
        result->setAuxiliaryText(std::string(kGenericRawNewAuxiliary));
    }
    for (const ast::GenericParameterSyntax& parameter : source->genericParameterSyntaxes())
    {
        ast::GenericParameterSyntax copy = parameter;
        copy.constantType = cloneType(arena, parameter.constantType, substitutions);
        result->addGenericParameter(std::move(copy));
    }
    for (const ast::LifetimeParameterSyntax& parameter : source->lifetimeParameters())
        result->addLifetimeParameter(parameter);
    for (const ast::LifetimeOutlivesConstraint& constraint : source->lifetimeOutlivesConstraints())
        result->addLifetimeOutlivesConstraint(constraint);
    for (const ast::GenericConstraintClause& clause : source->genericConstraints())
    {
        ast::GenericConstraintClause copy;
        copy.parameterName = clause.parameterName;
        copy.parameterRange = clause.parameterRange;
        copy.requiresClass = clause.requiresClass;
        copy.requiresValueType = clause.requiresValueType;
        copy.requiresDefaultConstructor = clause.requiresDefaultConstructor;
        copy.requiresCopy = clause.requiresCopy;
        copy.requiresMove = clause.requiresMove;
        copy.requiresTrackedLifetime = clause.requiresTrackedLifetime;
        copy.requiresUnmanaged = clause.requiresUnmanaged;
        copy.requiresNotNull = clause.requiresNotNull;
        copy.requiresRefLike = clause.requiresRefLike;
        copy.equalityConstraint = cloneType(arena, clause.equalityConstraint, substitutions);
        copy.associatedProjection = cloneType(
            arena, clause.associatedProjection, substitutions);
        copy.range = clause.range;
        for (const ast::TypeSyntax* constraint : clause.typeConstraints)
            copy.typeConstraints.push_back(cloneType(arena, constraint, substitutions));
        if (copy.associatedProjection != nullptr)
        {
            copy.substitutedArgument = copy.associatedProjection;
            preserveOrAttachInstantiationOrigin(
                const_cast<ast::TypeSyntax*>(copy.substitutedArgument));
        }
        else if (const auto found = substitutions.find(clause.parameterName);
                 found != substitutions.end())
        {
            ast::TypeSyntax* substitutedArgument = cloneType(arena, found->second, substitutions);
            preserveOrAttachInstantiationOrigin(substitutedArgument);
            copy.substitutedArgument = substitutedArgument;
        }
        result->addGenericConstraint(std::move(copy));
    }
    if (!rewrittenContainingTypeName.empty() && !originalContainingTypeName.empty() &&
        (source->kind() == ast::AstNodeKind::ConstructorDeclaration ||
         source->kind() == ast::AstNodeKind::DestructorDeclaration) &&
        source->text() == originalContainingTypeName)
    {
        result->setText(std::string(rewrittenContainingTypeName));
    }
    if (layoutQueryOfSubstitutedParameter)
    {
        ast::TypeSyntax* measuredType = cloneType(
            arena, substitutions.at(source->first()->text()), substitutions);
        preserveOrAttachInstantiationOrigin(measuredType);
        result->setTypeSyntax(measuredType);
    }
    else
    {
        result->setTypeSyntax(cloneType(arena, source->typeSyntax(), substitutions));
    }
    result->setThrowsTypeSyntax(
        cloneType(arena, source->throwsTypeSyntax(), substitutions));
    for (const ast::TypeSyntax* syntax : source->typeSyntaxes())
        for (ast::TypeSyntax* expanded :
             cloneExpandedTypeArgument(arena, syntax, substitutions))
            result->addTypeSyntax(expanded);
    return result;
}

// NSC305-G06. Clones the special (non-generic) node cases (see the header).
ast::AstNode* ProjectGenericModel::tryCloneSpecialNode(
    ast::AstArena& arena,
    const ast::AstNode* source,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    std::string_view rewrittenContainingTypeName,
    std::string_view originalContainingTypeName)
{
    if (source->kind() == ast::AstNodeKind::MemberExpression &&
        source->first() != nullptr &&
        source->first()->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        const auto substitution = substitutions.find(source->first()->text());
        if (substitution != substitutions.end())
        {
            const ast::TypeSyntax* ownerType = substitution->second;
            const std::string owner = typeSpelling(ownerType);
            std::vector<std::string> candidates{owner + "." + source->text()};
            if (ownerType != nullptr)
            {
                if (const auto identity = resolvedTypeIdentityBySyntax_.find(ownerType);
                    identity != resolvedTypeIdentityBySyntax_.end())
                {
                    const std::size_t moduleSeparator = identity->second.find("::");
                    if (moduleSeparator != std::string::npos)
                        candidates.push_back(identity->second.substr(moduleSeparator + 2U) +
                            "." + source->text());
                }
            }
            if (ownerType != nullptr && ownerType->originNamespace().has_value() &&
                !ownerType->originNamespace()->empty())
                candidates.push_back(namespaceAppend(
                    *ownerType->originNamespace(), owner) + "." + source->text());
            if (!currentInstantiationNamespace_.empty())
                candidates.push_back(namespaceAppend(
                    currentInstantiationNamespace_, owner) + "." + source->text());
            for (const std::string& candidate : candidates)
            {
                const auto found = associatedConstants_.find(candidate);
                if (found == associatedConstants_.end() || found->second == nullptr ||
                    found->second->first() == nullptr)
                    continue;
                const std::string activeKey = "const:" + candidate;
                if (!activeAssociatedProjections_.insert(activeKey).second)
                {
                    if (associatedProjectionError_.empty())
                    {
                        associatedProjectionError_ =
                            "associated constant normalization cycle reaches '" + candidate + "'";
                        associatedProjectionErrorRange_ = source->range();
                    }
                    break;
                }
                ast::AstNode* result = cloneNode(
                    arena, found->second->first(), substitutions,
                    rewrittenContainingTypeName, originalContainingTypeName);
                activeAssociatedProjections_.erase(activeKey);
                return result;
            }
        }
    }
    if (ast::AstNode* folded = foldValuePack(
            arena, source, substitutions,
            rewrittenContainingTypeName, originalContainingTypeName))
        return folded;
    if (source->kind() == ast::AstNodeKind::IdentifierExpression)
    {
        const auto substitution = substitutions.find(source->text());
        std::string constType;
        std::string constValue;
        if (substitution != substitutions.end() &&
            decodeConstGenericArgument(substitution->second, constType, constValue))
        {
            if (ast::AstNode* expression = constArgumentExpression(
                    arena, constType, constValue, source->range()))
                return expression;
        }
    }
    return nullptr;
}

ast::AstNode* ProjectGenericModel::cloneNode(
    ast::AstArena& arena,
    const ast::AstNode* source,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    std::string_view rewrittenContainingTypeName,
    std::string_view originalContainingTypeName)
{
    if (source == nullptr) return nullptr;
    CloneDepthGuard depthGuard(cloneDepth_);
    if (cloneDepth_ > kMaximumCloneDepth)
    {
        if (packMaterializationError_.empty())
        {
            packMaterializationError_ =
                "generic materialization exceeds the maximum nesting depth";
            packMaterializationErrorRange_ = source->range();
        }
        return nullptr;
    }
    telemetry_.noteNodesCloned();
    if (ast::AstNode* special = tryCloneSpecialNode(
            arena, source, substitutions,
            rewrittenContainingTypeName, originalContainingTypeName))
        return special;
    const bool layoutQueryOfSubstitutedParameter =
        (source->kind() == ast::AstNodeKind::SizeofExpression ||
         source->kind() == ast::AstNodeKind::AlignofExpression) &&
        source->first() != nullptr &&
        source->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
        substitutions.contains(source->first()->text());
    ast::AstNode* result = cloneNodeGeneric(
        arena, source, substitutions, rewrittenContainingTypeName, originalContainingTypeName);
    // NSC305-G06. Iteratively clone the child subtree with an explicit stack. Special cases are
    // handled by tryCloneSpecialNode; generic children are shallow-cloned here and their own
    // children pushed, so deeply nested generic expressions no longer recurse.
    const auto isLayoutQuery = [&](const ast::AstNode* node)
    {
        return node != nullptr &&
            (node->kind() == ast::AstNodeKind::SizeofExpression ||
             node->kind() == ast::AstNodeKind::AlignofExpression) &&
            node->first() != nullptr &&
            node->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
            substitutions.contains(node->first()->text());
    };
    struct ChildWork
    {
        const ast::AstNode* source;
        ast::AstNode* parent;
        std::uint8_t slot;  // 1=first, 2=second, 3=third, 4=child
    };
    std::vector<ChildWork> work;
    for (auto it = source->children().rbegin(); it != source->children().rend(); ++it)
        work.push_back(ChildWork{*it, result, 4});
    if (source->third() != nullptr)
        work.push_back(ChildWork{source->third(), result, 3});
    if (source->second() != nullptr)
        work.push_back(ChildWork{source->second(), result, 2});
    if (!layoutQueryOfSubstitutedParameter)
        work.push_back(ChildWork{source->first(), result, 1});
    while (!work.empty())
    {
        ChildWork item = work.back();
        work.pop_back();
        ast::AstNode* child = nullptr;
        if (item.source != nullptr)
        {
            child = tryCloneSpecialNode(
                arena, item.source, substitutions,
                rewrittenContainingTypeName, originalContainingTypeName);
            if (child == nullptr)
            {
                child = cloneNodeGeneric(
                    arena, item.source, substitutions,
                    rewrittenContainingTypeName, originalContainingTypeName);
                for (auto it = item.source->children().rbegin();
                     it != item.source->children().rend(); ++it)
                    work.push_back(ChildWork{*it, child, 4});
                if (item.source->third() != nullptr)
                    work.push_back(ChildWork{item.source->third(), child, 3});
                if (item.source->second() != nullptr)
                    work.push_back(ChildWork{item.source->second(), child, 2});
                if (!isLayoutQuery(item.source))
                    work.push_back(ChildWork{item.source->first(), child, 1});
            }
        }
        switch (item.slot)
        {
            case 1: item.parent->setFirst(child); break;
            case 2: item.parent->setSecond(child); break;
            case 3: item.parent->setThird(child); break;
            default: if (child != nullptr) item.parent->addChild(child); break;
        }
    }
    return result;
}

std::string ProjectGenericModel::typeSpelling(const ast::TypeSyntax* syntax) const
{
    if (syntax == nullptr) return "<missing>";
    std::string constType;
    std::string constValue;
    if (decodeConstGenericArgument(syntax, constType, constValue))
        return "const(" + constType + "=" + constValue + ")";
    std::string result;
    if (syntax->isFunction())
    {
        language::FunctionType function;
        function.carrier = syntax->functionCarrier() ==
                ast::FunctionTypeCarrierSyntaxKind::Pointer
            ? language::FunctionCarrier::Pointer
            : language::FunctionCarrier::Reference;
        switch (syntax->functionCallingConvention())
        {
            case ast::FunctionCallingConventionSyntaxKind::NativeSharp:
                function.callingConvention = language::CallingConvention::NativeSharp; break;
            case ast::FunctionCallingConventionSyntaxKind::C:
                function.callingConvention = language::CallingConvention::C; break;
            case ast::FunctionCallingConventionSyntaxKind::StdCall:
                function.callingConvention = language::CallingConvention::StdCall; break;
            case ast::FunctionCallingConventionSyntaxKind::FastCall:
                function.callingConvention = language::CallingConvention::FastCall; break;
            case ast::FunctionCallingConventionSyntaxKind::VectorCall:
                function.callingConvention = language::CallingConvention::VectorCall; break;
            case ast::FunctionCallingConventionSyntaxKind::Win64:
                function.callingConvention = language::CallingConvention::Win64; break;
        }
        for (const ast::TypeSyntax* parameter : syntax->typeArguments())
            function.parameters.push_back(typeSpelling(parameter));
        function.result = typeSpelling(syntax->functionReturnType());
        function.errorType = syntax->functionErrorType() == nullptr
            ? std::string{} : typeSpelling(syntax->functionErrorType());
        function.lifetime = syntax->functionLifetimeName();
        function.variadic = syntax->functionIsVariadic();
        function.isUnsafe = syntax->functionIsUnsafe();
        function.isNoHeap = syntax->functionIsNoHeap();
        function.isAsync = syntax->functionIsAsync();
        function.nullable = syntax->functionIsNullable();
        result = language::functionTypeIdentity(function);
    }
    else if (syntax->isTuple())
    {
        result.push_back('(');
        for (std::size_t index = 0; index < syntax->typeArguments().size(); ++index)
        {
            if (index != 0) result.push_back(',');
            result += typeSpelling(syntax->typeArguments()[index]);
        }
        result.push_back(')');
    }
    else result = syntax->baseName();
    if (!syntax->isFunction() && !syntax->isTuple() &&
        !syntax->typeArguments().empty())
    {
        result.push_back('<');
        for (std::size_t index = 0; index < syntax->typeArguments().size(); ++index)
        {
            if (index != 0) result.push_back(',');
            result += typeSpelling(syntax->typeArguments()[index]);
        }
        result.push_back('>');
    }
    for (const ast::TypeModifierSyntax& modifier : syntax->modifiers())
    {
        if (modifier.kind == ast::TypeModifierSyntaxKind::Pointer) result.push_back('*');
        else if (modifier.kind == ast::TypeModifierSyntaxKind::DynamicArray) result += "[]";
        else if (modifier.kind == ast::TypeModifierSyntaxKind::RangeArray)
            result += '[' + std::string(
                modifier.arrayRank > 0 ? modifier.arrayRank - 1 : 0, ',') + ']';
        else if (modifier.kind == ast::TypeModifierSyntaxKind::FlexibleArray) result += "[*]";
        else result += "[fixed]";
    }
    if (syntax->isPackExpansion()) result += "...";
    return result;
}

std::string ProjectGenericModel::typeSpecializationIdentity(const ast::TypeSyntax* syntax) const
{
    std::string constType;
    std::string constValue;
    if (decodeConstGenericArgument(syntax, constType, constValue))
        return "const:" + constType + "=" + constValue;
    if (syntax != nullptr)
    {
        if (const auto resolved = resolvedTypeIdentityBySyntax_.find(syntax);
            resolved != resolvedTypeIdentityBySyntax_.end())
        {
            return resolved->second;
        }
    }

    const std::string spelling = typeSpelling(syntax);
    if (syntax == nullptr || lexer::isBuiltinTypeKeyword(syntax->baseTokenKind()))
    {
        return spelling;
    }

    // The spelling remains the public/readable type argument. The cache identity additionally
    // records where an unresolved source name came from, so AppA.Payload and AppB.Payload cannot
    // reuse one clone merely because both were written as `Payload`. A substituted argument
    // carries its original site explicitly; a direct call uses the current site. The persistent
    // identity uses the module name, never the graph-order-dependent ordinal (the ordinal remains
    // lookup-only). Length-prefix module and namespace so punctuation cannot make identities
    // concatenate alike.
    const std::size_t projectOrdinal =
        syntax->originProjectOrdinal().value_or(currentInstantiationProjectOrdinal_);
    const std::string_view moduleName = projectOrdinal < projectModuleNames_.size()
        ? std::string_view(projectModuleNames_[projectOrdinal])
        : std::string_view("<invalid-project>");
    const std::string namespaceName = syntax->originNamespace().has_value()
        ? syntax->originNamespace().value()
        : (syntax->originProjectOrdinal().has_value() ? std::string{} : currentInstantiationNamespace_);

    return spelling + "@origin-module[" + std::to_string(moduleName.size()) + "]=" +
        std::string(moduleName) +
        ";namespace[" + std::to_string(namespaceName.size()) + "]=" + namespaceName;
}

const ProjectGenericModel::DeclaredTypeIdentity* ProjectGenericModel::resolveDeclaredTypeIdentity(
    const ast::TypeSyntax* syntax,
    std::size_t projectOrdinal,
    std::string_view namespaceName,
    const GenericLookupScope& scope) const
{
    if (syntax == nullptr || syntax->isFunction() ||
        lexer::isBuiltinTypeKeyword(syntax->baseTokenKind()) ||
        !syntax->typeArguments().empty())
    {
        return nullptr;
    }
    // Module-qualified names are an exact identity request, not a namespace-relative spelling. The
    // ordinary ProjectNameLookup already honors this form, and generic specialization must do so
    // before it builds its cache key. Otherwise a reconstructed type such as
    // Provider::Contracts.Value falls back to a source-origin spelling and produces a different
    // closed generic name from the producer that published it.
    const std::string& writtenName = syntax->baseName();
    if (const std::size_t moduleSeparator = writtenName.find("::");
        moduleSeparator != std::string::npos)
    {
        const std::string_view requestedModule(writtenName.data(), moduleSeparator);
        const std::string_view requestedType(
            writtenName.data() + moduleSeparator + 2U,
            writtenName.size() - moduleSeparator - 2U);
        if (requestedModule.empty() || requestedType.empty()) return nullptr;

        const DeclaredTypeIdentity* exact = nullptr;
        for (const DeclaredTypeIdentity& declaration : declaredTypeIdentities_)
        {
            if (declaration.sourceName != requestedType ||
                !declaration.identityKey.starts_with(std::string(requestedModule) + "::") ||
                (declaration.projectOrdinal != projectOrdinal && !declaration.isPublic))
                continue;
            if (exact != nullptr) return nullptr;
            exact = &declaration;
        }
        return exact;
    }

    projectOrdinal = syntax->originProjectOrdinal().value_or(projectOrdinal);
    const std::string effectiveNamespace = syntax->originNamespace().has_value()
        ? syntax->originNamespace().value()
        : (syntax->originProjectOrdinal().has_value() ? std::string{} : std::string(namespaceName));
    const std::vector<std::string> namespaceSegments = splitDotted(effectiveNamespace);
    const std::vector<std::string> noContainingTypes;

    // A substituted node carries the original project/namespace, but not its using directives.
    // Its declaration identity was normally copied before substitution. If an older/unannotated
    // node reaches this fallback, do not accidentally apply the template's imports to the
    // consumer's source name.
    const std::vector<std::string> noImports;
    const std::unordered_map<std::string, std::string> noAliases;
    QualifiedNameSite site;
    site.namespaceSegments = namespaceSegments;
    site.enclosingTypeNames = noContainingTypes;
    site.aliases = syntax->originProjectOrdinal().has_value() ? &noAliases : &scope.aliases.read();
    site.importedNamespaces = syntax->originProjectOrdinal().has_value()
        ? std::span<const std::string>(noImports)
        : std::span<const std::string>(scope.importedNamespaces.read());

    for (const QualifiedNameRung& rung : qualifiedNameLadder(site, syntax->baseName()))
    {
        std::vector<const DeclaredTypeIdentity*> identities;
        for (const std::string& spelling : rung.spellings)
        {
            for (const DeclaredTypeIdentity& declaration : declaredTypeIdentities_)
            {
                if (declaration.sourceName != spelling) continue;

                const bool sameProject = declaration.projectOrdinal == projectOrdinal;
                bool visible = false;
                switch (rung.tier)
                {
                    case QualifiedNameTier::EnclosingType:
                    case QualifiedNameTier::LexicalNamespace:
                    case QualifiedNameTier::Global:
                        visible = sameProject;
                        break;
                    case QualifiedNameTier::ImportedNamespace:
                        visible = sameProject ||
                            (declaration.isPublic &&
                             projectVisible(projectOrdinal, declaration.projectOrdinal));
                        break;
                    case QualifiedNameTier::ReferencedModule:
                        visible = !sameProject && declaration.isPublic &&
                            projectVisible(projectOrdinal, declaration.projectOrdinal);
                        break;
                }
                if (!visible || std::find_if(
                        identities.begin(), identities.end(), [&](const DeclaredTypeIdentity* identity) {
                            return identity != nullptr && identity->identityKey == declaration.identityKey;
                        }) != identities.end())
                {
                    continue;
                }
                identities.push_back(&declaration);
            }
        }
        if (identities.size() == 1) return identities.front();
        if (identities.size() > 1) return nullptr;
    }
    return nullptr;
}

void ProjectGenericModel::appendTypeModifiers(
    std::string& identity,
    const ast::TypeSyntax* syntax)
{
    if (syntax == nullptr) return;
    for (const ast::TypeModifierSyntax& modifier : syntax->modifiers())
    {
        if (modifier.kind == ast::TypeModifierSyntaxKind::Pointer) identity.push_back('*');
        else if (modifier.kind == ast::TypeModifierSyntaxKind::DynamicArray) identity += "[]";
        else if (modifier.kind == ast::TypeModifierSyntaxKind::RangeArray)
            identity += '[' + std::string(
                modifier.arrayRank > 0 ? modifier.arrayRank - 1 : 0, ',') + ']';
        else if (modifier.kind == ast::TypeModifierSyntaxKind::FlexibleArray) identity += "[*]";
        else identity += "[fixed]";
    }
}

std::string ProjectGenericModel::namespaceAppend(std::string_view prefix, std::string_view suffix)
{
    if (prefix.empty()) return std::string(suffix);
    if (suffix.empty()) return std::string(prefix);
    return std::string(prefix) + "." + std::string(suffix);
}

std::string ProjectGenericModel::sanitize(std::string_view text)
{
    std::string result;
    for (const char raw : text)
    {
        const unsigned char ch = static_cast<unsigned char>(raw);
        if (std::isalnum(ch) != 0) result.push_back(static_cast<char>(ch));
        else result.push_back('_');
    }
    while (!result.empty() && result.back() == '_') result.pop_back();
    if (result.empty()) result = "type";
    return result;
}

std::string ProjectGenericModel::methodCalleeName(const ast::AstNode* call)
{
    if (call == nullptr || call->first() == nullptr) return {};
    const ast::AstNode* callee = call->first();
    if (callee->kind() == ast::AstNodeKind::IdentifierExpression ||
        callee->kind() == ast::AstNodeKind::MemberExpression)
        return callee->text();
    return {};
}

ast::AstNode* ProjectGenericModel::methodCalleeSyntax(ast::AstNode* call)
{
    if (call == nullptr) return nullptr;
    return call->first();
}
void ProjectGenericModel::buildVisibility(const compilation::ProjectGraph& graph)
{
    const auto projects = graph.projects();
    projectModuleNames_.clear();
    projectModuleNames_.reserve(projects.size());
    for (const compilation::Project& project : projects)
    {
        projectModuleNames_.push_back(project.moduleName());
    }
    visibleProjects_.assign(projects.size(), {});
    std::unordered_map<std::string, std::size_t> projectByPath;
    const auto pathKey = [](const std::filesystem::path& path)
    {
        std::error_code errorCode;
        std::filesystem::path canonical = std::filesystem::weakly_canonical(path, errorCode);
        if (errorCode) canonical = std::filesystem::absolute(path).lexically_normal();
        std::string key = canonical.generic_string();
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return key;
    };
    for (std::size_t ordinal = 0; ordinal < projects.size(); ++ordinal)
        projectByPath.emplace(pathKey(projects[ordinal].projectFile()), ordinal);

    for (std::size_t start = 0; start < projects.size(); ++start)
    {
        std::vector<bool> reachable(projects.size(), false);
        std::function<void(std::size_t)> visit = [&](std::size_t ordinal)
        {
            for (const std::filesystem::path& reference : projects[ordinal].projectReferences())
            {
                const auto found = projectByPath.find(pathKey(reference));
                if (found == projectByPath.end() || reachable[found->second]) continue;
                reachable[found->second] = true;
                visit(found->second);
            }
        };
        visit(start);
        for (std::size_t ordinal = 0; ordinal < reachable.size(); ++ordinal)
            if (reachable[ordinal]) visibleProjects_[start].push_back(ordinal);
    }
}

bool ProjectGenericModel::projectVisible(std::size_t from, std::size_t target) const noexcept
{
    if (from == target) return true;
    if (from >= visibleProjects_.size()) return false;
    return std::find(visibleProjects_[from].begin(), visibleProjects_[from].end(), target) !=
        visibleProjects_[from].end();
}

ProjectGenericModel::TemplateState* ProjectGenericModel::stateForSyntax(
    const ast::AstNode* syntax) noexcept
{
    const auto found = stateBySyntax_.find(syntax);
    return found == stateBySyntax_.end() ? nullptr : found->second;
}

void ProjectGenericModel::discoverTemplateDependencies(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    std::string namespaceName,
    std::string ownerName,
    GenericLookupScope scope)
{
    // NSC305-G05. Iterative pre-order traversal with an explicit stack; the per-node logic is
    // unchanged, only the recursion is removed.
    struct DependencyFrame
    {
        ast::AstNode* node;
        std::string namespaceName;
        std::string ownerName;
        GenericLookupScope scope;
    };
    std::vector<DependencyFrame> stack;
    stack.push_back(DependencyFrame{node, std::move(namespaceName), std::move(ownerName),
        std::move(scope)});
    while (!stack.empty())
    {
        DependencyFrame frame = std::move(stack.back());
        stack.pop_back();
        node = frame.node;
        namespaceName = std::move(frame.namespaceName);
        ownerName = std::move(frame.ownerName);
        scope = std::move(frame.scope);
        if (node == nullptr) continue;

    std::string currentNamespace = namespaceName;
    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
        currentNamespace = namespaceAppend(namespaceName, node->text());

    GenericLookupScope currentScope = scope;
    for (const ast::AstNode* child : node->children())
    {
        if (child == nullptr) continue;
        if (child->kind() == ast::AstNodeKind::UsingNamespaceDirective)
        {
            const std::string written = child->text();
            if (written.empty()) continue;
            const auto add = [&](std::string candidate)
            {
                auto& imported = currentScope.importedNamespaces.write();
                if (std::find(imported.begin(), imported.end(), candidate) == imported.end())
                    imported.push_back(std::move(candidate));
            };
            const std::vector<std::string> segments = splitDotted(currentNamespace);
            for (std::size_t depth = segments.size(); depth > 0; --depth)
            {
                std::string prefixed;
                for (std::size_t index = 0; index < depth; ++index)
                {
                    if (!prefixed.empty()) prefixed.push_back('.');
                    prefixed += segments[index];
                }
                prefixed.push_back('.');
                prefixed += written;
                add(std::move(prefixed));
            }
            add(written);
        }
        else if (child->kind() == ast::AstNodeKind::UsingAliasDirective &&
                 !child->auxiliaryText().empty())
        {
            currentScope.aliases.write()[child->text()] = child->auxiliaryText();
        }
    }

    if (TemplateState* root = stateForSyntax(node); root != nullptr)
    {
        const std::string templateOwner = root->record.kind == GenericTemplateKind::Method
            ? ownerName
            : appendOwner(ownerName, node->text());
        collectTemplateDependencies(
            *root, unit, node, currentNamespace, templateOwner, currentScope, node);
        continue;
    }

    const std::string childOwner =
        (node->kind() == ast::AstNodeKind::ClassDeclaration ||
         node->kind() == ast::AstNodeKind::StructDeclaration)
            ? appendOwner(ownerName, node->text())
            : ownerName;
    const std::vector<ast::AstNode*> children(node->children().begin(), node->children().end());
    for (auto iterator = children.rbegin(); iterator != children.rend(); ++iterator)
        stack.push_back(DependencyFrame{*iterator, currentNamespace, childOwner, currentScope});
    }
}

void ProjectGenericModel::collectTypeDependency(
    TemplateState& root,
    ProjectSyntaxUnit& unit,
    const ast::TypeSyntax* syntax,
    const std::string& namespaceName,
    const GenericLookupScope& scope)
{
    // NSC305-G05. Iterative traversal of the type-argument tree. The original recursed into
    // arguments first; because dependencies, unresolved dependencies and external references are
    // sorted and deduped later in build(), the visit order does not affect the final result, so a
    // plain stack suffices.
    std::vector<const ast::TypeSyntax*> stack;
    stack.push_back(syntax);
    while (!stack.empty())
    {
        const ast::TypeSyntax* current = stack.back();
        stack.pop_back();
        if (current == nullptr) continue;
        for (const ast::TypeSyntax* argument : current->typeArguments())
            stack.push_back(argument);
        // Future/Iterator/AsyncIterator, tuples, and portable SIMD are structural surfaces. Their
        // arguments may depend on nominal templates, but the surface itself has no template record
        // that a reusable NSMETA closure could or should include.
        if (isStructuralGenericSurface(current)) continue;
        if (current->typeArguments().empty())
        {
            if (const DeclaredTypeIdentity* declaration = resolveDeclaredTypeIdentity(
                    current, unit.projectOrdinal(), namespaceName, scope);
                declaration != nullptr &&
                !declaration->identityKey.starts_with(root.record.moduleName + "::"))
            {
                root.record.externalTypeReferences.push_back(
                    {current->range(), "type:" + declaration->identityKey});
            }
            continue;
        }

        TypeResolution resolution = resolveTypeTemplate(
            unit, namespaceName, current->baseName(), current->typeArguments().size());
        if (resolution.kind != TypeResolutionKind::Unique)
            resolution = resolveTypeTemplate(
                unit, namespaceName, current->baseName(),
                std::numeric_limits<std::size_t>::max());
        TemplateState* target =
            resolution.kind == TypeResolutionKind::Unique ? resolution.state : nullptr;
        if (target == nullptr)
        {
            root.record.unresolvedDependencies.push_back(
                "generic type '" + current->baseName() + "<...>'");
            continue;
        }
        if (target->record.moduleName != root.record.moduleName)
        {
            root.record.externalTypeReferences.push_back({
                current->range(),
                "type:" + target->record.canonicalKey});
        }
        root.record.dependencies.push_back(target->record.canonicalKey);
    }
}

void ProjectGenericModel::collectCallDependency(
    TemplateState& root,
    ProjectSyntaxUnit& unit,
    const ast::AstNode* call,
    const std::string& namespaceName,
    const std::string& ownerName,
    const GenericLookupScope& scope)
{
    if (call == nullptr || call->kind() != ast::AstNodeKind::CallExpression ||
        call->typeSyntaxes().empty()) return;

    const ast::AstNode* callee = call->first();
    const std::string methodName = methodCalleeName(call);
    std::string requestedOwner = templateSourceSpelling(namespaceName, ownerName);
    if (callee != nullptr && callee->kind() == ast::AstNodeKind::MemberExpression)
    {
        const std::string written = receiverSourceSpelling(callee->first());
        if (!written.empty()) requestedOwner = written;
    }

    QualifiedNameSiteContext siteContext = makeQualifiedNameSite(namespaceName, ownerName, scope);
    const QualifiedNameSite& site = siteContext.site;

    const std::size_t arity = call->typeSyntaxes().size();
    const std::size_t argumentCount = call->children().size();
    // NSC305-G03. Build the name ladder once; every arity/argument-count variation reuses it.
    const std::vector<QualifiedNameRung> ladder = qualifiedNameLadder(site, requestedOwner);
    const std::vector<TemplateState*> named = templatesNamedBy(
        unit, site, requestedOwner, methodName, arity, argumentCount, &ladder);
    const std::vector<TemplateState*> targets = collectMethodTemplatesFromNamed(named);
    if (!targets.empty())
    {
        for (const TemplateState* target : targets)
            root.record.dependencies.push_back(target->record.canonicalKey);
        return;
    }

    TemplateState* fallback = resolveMethodTemplate(
        unit, site, requestedOwner, methodName, arity, argumentCount, &named, &ladder);
    if (fallback == nullptr)
        fallback = resolveMethodTemplate(
            unit, site, requestedOwner, methodName,
            std::numeric_limits<std::size_t>::max(), argumentCount, nullptr, &ladder);
    if (fallback != nullptr)
    {
        root.record.dependencies.push_back(fallback->record.canonicalKey);
        return;
    }

    root.record.unresolvedDependencies.push_back(
        std::string(named.size() > 1U ? "ambiguous generic method '" : "missing generic method '") +
        methodName + "<...>'");
}

void ProjectGenericModel::collectTemplateDependencies(
    TemplateState& root,
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    std::string namespaceName,
    std::string ownerName,
    GenericLookupScope scope,
    const ast::AstNode* rootSyntax)
{
    // NSC305-G05. Iterative pre-order traversal with an explicit stack; the per-node logic is
    // unchanged, only the recursion into first/second/third/children is removed.
    struct BodyFrame
    {
        ast::AstNode* node;
        std::string namespaceName;
        std::string ownerName;
        GenericLookupScope scope;
    };
    std::vector<BodyFrame> stack;
    stack.push_back(
        BodyFrame{node, std::move(namespaceName), std::move(ownerName), std::move(scope)});
    while (!stack.empty())
    {
        BodyFrame frame = std::move(stack.back());
        stack.pop_back();
        node = frame.node;
        namespaceName = std::move(frame.namespaceName);
        ownerName = std::move(frame.ownerName);
        scope = std::move(frame.scope);
        if (node == nullptr) continue;
        if (node != rootSyntax && stateForSyntax(node) != nullptr) continue;

    std::string currentNamespace = namespaceName;
    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
        currentNamespace = namespaceAppend(namespaceName, node->text());

    GenericLookupScope currentScope = scope;
    for (const ast::AstNode* child : node->children())
    {
        if (child == nullptr) continue;
        if (child->kind() == ast::AstNodeKind::UsingNamespaceDirective)
            currentScope.importedNamespaces.write().push_back(child->text());
        else if (child->kind() == ast::AstNodeKind::UsingAliasDirective &&
                 !child->auxiliaryText().empty())
            currentScope.aliases.write()[child->text()] = child->auxiliaryText();
    }

    collectTypeDependency(root, unit, node->typeSyntax(), currentNamespace, currentScope);
    for (const ast::TypeSyntax* syntax : node->typeSyntaxes())
        collectTypeDependency(root, unit, syntax, currentNamespace, currentScope);
    for (const ast::GenericConstraintClause& clause : node->genericConstraints())
    {
        for (const ast::TypeSyntax* constraint : clause.typeConstraints)
            collectTypeDependency(root, unit, constraint, currentNamespace, currentScope);
        collectTypeDependency(
            root, unit, clause.equalityConstraint, currentNamespace, currentScope);
        collectTypeDependency(
            root, unit, clause.substitutedArgument, currentNamespace, currentScope);
    }
    collectCallDependency(
        root, unit, node, currentNamespace, ownerName, currentScope);

    const std::string childOwner = node == rootSyntax
        ? ownerName
        : ((node->kind() == ast::AstNodeKind::ClassDeclaration ||
            node->kind() == ast::AstNodeKind::StructDeclaration)
               ? appendOwner(ownerName, node->text())
               : ownerName);
    const std::vector<ast::AstNode*> children(node->children().begin(), node->children().end());
    for (auto iterator = children.rbegin(); iterator != children.rend(); ++iterator)
        stack.push_back(BodyFrame{*iterator, currentNamespace, childOwner, currentScope});
    if (node->third() != nullptr)
        stack.push_back(BodyFrame{node->third(), currentNamespace, ownerName, currentScope});
    if (node->second() != nullptr)
        stack.push_back(BodyFrame{node->second(), currentNamespace, ownerName, currentScope});
    if (node->first() != nullptr)
        stack.push_back(BodyFrame{node->first(), currentNamespace, ownerName, currentScope});
    }
}

bool ProjectGenericModel::templateVisible(
    std::size_t from,
    const GenericTemplateRecord& target) const noexcept
{
    // A materialization-only declaration remains usable while walking a clone owned by its
    // producer project. A different project may discover only the producer's public roots.
    return from == target.projectOrdinal ||
        (target.accessibility == GenericTemplateAccessibility::Public &&
         projectVisible(from, target.projectOrdinal));
}

}
