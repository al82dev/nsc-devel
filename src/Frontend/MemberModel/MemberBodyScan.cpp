#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "../Internal/EnumInitializerResolver.h"
#include "../Internal/AnonymousMemberPromotionVisitState.h"
#include "../Internal/EnumValue.h"
#include "../Internal/EnumSuccessor.h"
#include "Internal/MemberBodyScanWorkKind.h"
#include "Internal/MemberBodyScanWorkItem.h"

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
void ProjectMemberModel::completeThisMemberAccess(
    const ast::AstNode* node,
    std::size_t typeOrdinal,
    std::size_t methodMemberOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::span<const std::size_t> directCandidates =
        findMembers(typeOrdinal, node->text());
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
}

void ProjectMemberModel::recordThisReference(
    const ast::AstNode* node,
    std::size_t typeOrdinal,
    std::size_t methodMemberOrdinal)
{
    bodyBindings_.push_back(MemberBodyBinding{
        MemberBodyBindingKind::ThisReference,
        node,
        typeOrdinal,
        methodMemberOrdinal,
        {},
        {}});
}

bool ProjectMemberModel::isDirectThisMemberAccess(const ast::AstNode* node) noexcept
{
    return node->kind() == ast::AstNodeKind::MemberExpression &&
        node->first() != nullptr &&
        node->first()->kind() == ast::AstNodeKind::ThisExpression;
}

bool ProjectMemberModel::rejectThisPointerMemberAccess(
    const ast::AstNode* node,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // IDs 80/83. `this->x`. Este rechazo estaba escrito contra PointerMemberExpression, el nodo
    // propio que el parser construia para `->` antes de que `p->x` pasara a construirse como
    // `(*p).x`. Al desaparecer el nodo, la comprobacion quedo muerta y `this->Value` empezo a
    // COMPILAR -- lo vio `nsc.project-member-model`, que es la unica prueba que mide esta fase.
    //
    // El mensaje se conserva palabra por palabra, y con el su DiagnosticId, porque es mejor que el
    // generico: no dice "no es un puntero", dice QUE ESCRIBIR en su lugar. Lo unico que cambia es
    // la forma que se reconoce: un acceso a miembro sobre el `*` SINTETIZADO que el parser marca
    // (auxiliaryText "arrow"), sobre `this`.
    if (node->kind() != ast::AstNodeKind::MemberExpression ||
        node->first() == nullptr ||
        node->first()->kind() != ast::AstNodeKind::UnaryExpression ||
        node->first()->auxiliaryText() != "arrow" ||
        node->first()->first() == nullptr ||
        node->first()->first()->kind() != ast::AstNodeKind::ThisExpression)
    {
        return false;
    }

    diagnostics.report(
        diagnostics::DiagnosticId::InvalidMemberAccess,
        diagnostics::DiagnosticSeverity::Error,
        node->range(),
        "'this' is a class reference; use '.' rather than '->' for member lookup");
    return true;
}

void ProjectMemberModel::scanMethodBody(
    const ast::AstNode* node,
    std::size_t typeOrdinal,
    std::size_t methodMemberOrdinal,
    diagnostics::DiagnosticEngine& diagnostics)
{
    using internal::WorkItem;
    using internal::WorkKind;
    std::vector<WorkItem> work;
    work.push_back(WorkItem{WorkKind::Visit, node});
    while (!work.empty())
    {
        const WorkItem item = work.back();
        work.pop_back();
        node = item.node;
        if (node == nullptr) continue;

        if (item.kind == WorkKind::CompleteThisMember)
        {
            completeThisMemberAccess(node, typeOrdinal, methodMemberOrdinal, diagnostics);
            continue;
        }

        if (node->kind() == ast::AstNodeKind::ThisExpression)
        {
            recordThisReference(node, typeOrdinal, methodMemberOrdinal);
            continue;
        }

        if (isDirectThisMemberAccess(node))
        {
            work.push_back(WorkItem{WorkKind::CompleteThisMember, node});
            work.push_back(WorkItem{WorkKind::Visit, node->first()});
            continue;
        }

        if (rejectThisPointerMemberAccess(node, diagnostics))
        {
            work.push_back(WorkItem{WorkKind::Visit, node->first()->first()});
            continue;
        }

        const std::span<ast::AstNode* const> children = node->children();
        for (auto iterator = children.rbegin(); iterator != children.rend(); ++iterator)
            work.push_back(WorkItem{WorkKind::Visit, *iterator});
        work.push_back(WorkItem{WorkKind::Visit, node->third()});
        work.push_back(WorkItem{WorkKind::Visit, node->second()});
        work.push_back(WorkItem{WorkKind::Visit, node->first()});
    }
}

void ProjectMemberModel::scanInvalidThisContexts(
    const ast::AstNode* node,
    diagnostics::DiagnosticEngine& diagnostics) const
{
    std::vector<const ast::AstNode*> work;
    work.push_back(node);
    while (!work.empty())
    {
        node = work.back();
        work.pop_back();
        if (node == nullptr) continue;

        if (node->kind() == ast::AstNodeKind::MethodDeclaration ||
            node->kind() == ast::AstNodeKind::ConstructorDeclaration ||
            node->kind() == ast::AstNodeKind::DestructorDeclaration)
        {
            continue;
        }

        if (node->kind() == ast::AstNodeKind::ThisExpression)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidMemberAccess,
                diagnostics::DiagnosticSeverity::Error,
                node->range(),
                "'this' is available only inside an instance method");
            continue;
        }

        const std::span<ast::AstNode* const> children = node->children();
        for (auto iterator = children.rbegin(); iterator != children.rend(); ++iterator)
            work.push_back(*iterator);
        work.push_back(node->third());
        work.push_back(node->second());
        work.push_back(node->first());
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
