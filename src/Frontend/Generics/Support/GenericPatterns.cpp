#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "../../Internal/CloneDepthGuard.h"
#include "../../Internal/ByteReader.h"
#include "../../Internal/EnumConstResolver.h"
#include "../../Internal/SpecializationWaveTimer.h"
#include "../../Internal/InterfaceFingerprint.h"
#include "../../Internal/ImplementationFingerprint.h"
#include "../../Internal/LocatedNode.h"
#include "../../Internal/MirFunctionKey.h"
#include "../../Internal/MirCacheRecord.h"
#include "../../Internal/ProjectGenericDependencyFrame.h"
#include "../../Internal/ProjectGenericBodyFrame.h"
#include "../../Internal/ProjectGenericAliasFrame.h"
#include "../../Internal/ProjectGenericChildWork.h"
#include "../../Internal/ProjectGenericCollectFrame.h"
#include "../../Internal/ProjectGenericProcessFrame.h"
#include "../../Internal/ProjectGenericReservedDemand.h"
#include "../../Internal/TypeWork.h"
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
#include <future>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "GenericPatterns.h"
#include "ConstGenericArguments.h"
namespace nativesharp::frontend::internal
{
static std::string genericExpressionPattern(const ast::AstNode* node)
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

static std::string genericTypePattern(
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
namespace nativesharp::frontend
{
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
}
