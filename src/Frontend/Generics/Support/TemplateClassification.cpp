#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "../../Internal/CloneDepthGuard.h"
#include "../../Cache/CachePayloadReader.h"
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
#include "TemplateClassification.h"
namespace nativesharp::frontend::internal
{
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
}
