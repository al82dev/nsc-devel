#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "../Internal/LookupEnvironment.h"
#include "../Internal/ProjectNameLookupFrame.h"

#include "nativesharp/Frontend/QualifiedNameLadder.h"

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/AstNodeKind.h"
#include "nativesharp/Compilation/PartialTypeAggregator.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"
#include "nativesharp/Driver/CompilationWorkerPool.h"
#include "nativesharp/Language/GenericSemantics.h"
#include "nativesharp/Language/AbiSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <deque>
#include <filesystem>
#include <functional>
#include <iterator>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace nativesharp::frontend
{
void ProjectNameLookup::bindUnit(
    const ProjectSyntaxUnit& unit,
    diagnostics::DiagnosticEngine& diagnostics,
    UnitBindingIndex& index)
{
    const LookupEnvironment environment;
    bindScope(unit, unit.syntaxTree(), {}, environment, diagnostics, index);
}

void ProjectNameLookup::bindUsingNamespaceDirective(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* directive,
    const std::vector<std::string>& namespaceSegments,
    LookupEnvironment& environment,
    diagnostics::DiagnosticEngine& diagnostics)
{
    const std::optional<std::string> resolved = resolveNamespaceTarget(
        unit.projectOrdinal(), namespaceSegments, directive->text());
    if (!resolved.has_value())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::UnknownNamespace,
            diagnostics::DiagnosticSeverity::Error,
            directive->range(),
            "unknown namespace '" + directive->text() + "'");
        return;
    }
    if (std::find(
            environment.importedNamespaces.begin(),
            environment.importedNamespaces.end(),
            *resolved) == environment.importedNamespaces.end())
    {
        environment.importedNamespaces.push_back(*resolved);
    }
}

void ProjectNameLookup::bindUsingDirectives(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* scopeNode,
    const std::vector<std::string>& namespaceSegments,
    LookupEnvironment& environment,
    diagnostics::DiagnosticEngine& diagnostics)
{
    std::unordered_set<std::string> localAliases;
    for (const ast::AstNode* child : scopeNode->children())
    {
        if (child->kind() == ast::AstNodeKind::UsingNamespaceDirective)
            bindUsingNamespaceDirective(
                unit, child, namespaceSegments, environment, diagnostics);
        else if (child->kind() == ast::AstNodeKind::UsingAliasDirective)
            bindUsingAliasDirective(
                unit, child, namespaceSegments, environment, localAliases, diagnostics);
    }
}

bool ProjectNameLookup::bindGenericTemplateConstraints(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& environment,
    diagnostics::DiagnosticEngine& diagnostics,
    UnitBindingIndex& index)
{
    if (node->genericParameters().empty()) return false;
    // NSC-LIMIT-007, segunda vuelta. UNA PLANTILLA TAMBIEN RESUELVE SUS
    // CONSTRAINTS, aunque nadie la instancie.
    //
    // Este `return` existe porque el cuerpo de una plantilla habla de `T`,
    // que no es un tipo y no se puede resolver; la especializacion clonada
    // es la que se visita. Pero los tipos ESCRITOS EN UN `where` no son
    // eso: `where T : IComponente` nombra a `IComponente`, que existe con
    // independencia de que `T` llegue a cerrarse. Sin esta vuelta, una
    // plantilla nunca instanciada se llevaba sus errores de DECLARACION
    // --`where T : MiEnum`-- sin que los viera nadie, y la promesa del ID
    // quedaba a medias: el error existia solo si alguien usaba la
    // plantilla.
    //
    // Solo los tipos de constraint: el argumento sustituido no existe en
    // una plantilla, y el parser ya rechaza que un constraint nombre otro
    // parametro generico, de modo que aqui no puede llegar por construccion
    // un nombre irresoluble.
    for (const ast::GenericConstraintClause& clause : node->genericConstraints())
    {
        for (const ast::TypeSyntax* constraint : clause.typeConstraints)
            bindTypeSyntax(unit, constraint, namespaceSegments, environment, diagnostics, index);
        if (clause.equalityConstraint == nullptr ||
            std::find(
                node->genericParameters().begin(),
                node->genericParameters().end(),
                std::string(clause.equalityConstraint->baseName())) !=
                node->genericParameters().end())
            continue;
        std::string constantType;
        std::string constantValue;
        if (!language::parseConstGenericArgumentMarker(
                clause.equalityConstraint->baseName(), constantType, constantValue))
            bindTypeSyntax(
                unit, clause.equalityConstraint, namespaceSegments, environment, diagnostics, index);
    }
    return true;
}

void ProjectNameLookup::bindSpecialNodeTypeReferences(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& environment,
    diagnostics::DiagnosticEngine& diagnostics,
    UnitBindingIndex& index)
{
    if (node->kind() == ast::AstNodeKind::MemberExpression)
    {
        bindExpressionTypeReceiver(unit, node->first(), namespaceSegments, environment, index);
        return;
    }
    if (node->kind() != ast::AstNodeKind::NameOfExpression) return;
    const ast::AstNode* target = node->first();
    while (target != nullptr && (target->kind() == ast::AstNodeKind::CallExpression ||
                                 target->kind() == ast::AstNodeKind::IndexExpression))
        target = target->first();
    if (node->typeSyntax() != nullptr && node->typeSyntax()->typeArguments().empty())
    {
        bindTypeSyntax(unit, node->typeSyntax(), namespaceSegments, environment, diagnostics, index);
        return;
    }
    bindExpressionTypeReceiver(unit, target, namespaceSegments, environment, index);
    if (node->typeSyntax() == nullptr) return;
    for (const ast::TypeSyntax* argument : node->typeSyntax()->typeArguments())
        bindTypeSyntax(unit, argument, namespaceSegments, environment, diagnostics, index);
}

void ProjectNameLookup::bindConstraintTypeReferences(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& environment,
    diagnostics::DiagnosticEngine& diagnostics,
    UnitBindingIndex& index)
{
    // NSC-LIMIT-007. Los tipos escritos en un `where` son referencias de tipo
    // como cualquier otra y se resuelven por el mismo camino: con el mismo
    // alcance, los mismos `using` y los mismos alias. Sin esta vuelta quedaban
    // sin resolver y la validacion de constraints solo veia
    // `unresolved:IComponente` -- que la habria obligado a llevar su propia
    // resolucion de nombres, o sea una segunda copia de la unica cosa que este
    // fichero hace.
    for (const ast::GenericConstraintClause& clause : node->genericConstraints())
    {
        for (const ast::TypeSyntax* constraint : clause.typeConstraints)
            bindTypeSyntax(unit, constraint, namespaceSegments, environment, diagnostics, index);
        if (clause.equalityConstraint != nullptr)
        {
            std::string constantType;
            std::string constantValue;
            if (!language::parseConstGenericArgumentMarker(
                    clause.equalityConstraint->baseName(), constantType, constantValue))
                bindTypeSyntax(
                    unit,
                    clause.equalityConstraint,
                    namespaceSegments,
                    environment,
                    diagnostics,
                    index);
        }
        std::string substitutedConstantType;
        std::string substitutedConstantValue;
        if (clause.substitutedArgument != nullptr &&
            !language::parseConstGenericArgumentMarker(
                clause.substitutedArgument->baseName(),
                substitutedConstantType,
                substitutedConstantValue))
            bindTypeSyntax(
                unit,
                clause.substitutedArgument,
                namespaceSegments,
                environment,
                diagnostics,
                index);
    }
}


void ProjectNameLookup::bindNode(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& environment,
    diagnostics::DiagnosticEngine& diagnostics,
    UnitBindingIndex& index)
{

    std::deque<LookupEnvironment> scopedEnvironments;
    std::vector<Frame> frames;
    frames.push_back(Frame{node, &environment, Stage::Enter});
    while (!frames.empty())
    {
        const Frame frame = frames.back();
        frames.pop_back();
        node = frame.node;
        if (node == nullptr || frame.environment == nullptr)
            continue;
        const LookupEnvironment* activeEnvironment = frame.environment;

        if (frame.stage == Stage::Enter)
        {
            if (bindGenericTemplateConstraints(
                    unit,
                    node,
                    namespaceSegments,
                    *activeEnvironment,
                    diagnostics,
                    index))
                continue;

            if (bindNamespaceDeclaration(
                    unit, node, namespaceSegments, *activeEnvironment, diagnostics, index))
                continue;

            // NSC-LIMIT-009. Entrar en un tipo AMPLIA el alcance: dentro de `Outer`,
            // `Item` se busca primero como `Outer.Item`, y un anidado `private` de
            // `Outer` se ve. El entorno se copia solo cuando de verdad se entra en un
            // tipo; en cualquier otro nodo se sigue usando el heredado, que es la
            // referencia que llega, sin copias por nodo del arbol.
            const std::optional<std::size_t> ordinal = enclosingTypeOrdinal(node);
            if (ordinal.has_value())
            {
                scopedEnvironments.push_back(*activeEnvironment);
                scopedEnvironments.back().enclosingTypeOrdinals.push_back(*ordinal);
                activeEnvironment = &scopedEnvironments.back();
            }
            const LookupEnvironment& scope = *activeEnvironment;
            bindSpecialNodeTypeReferences(
                unit, node, namespaceSegments, scope, diagnostics, index);

            if (node->typeSyntax() != nullptr && node->kind() != ast::AstNodeKind::NameOfExpression)
            {
                bindTypeSyntax(unit, node->typeSyntax(), namespaceSegments, scope, diagnostics, index);
                std::vector<const ast::AstNode*> arrayCounts;
                for (const ast::TypeModifierSyntax& modifier : node->typeSyntax()->modifiers())
                {
                    if (modifier.arrayCount != nullptr)
                        arrayCounts.push_back(modifier.arrayCount);
                }
                if (!arrayCounts.empty())
                {
                    frames.push_back(Frame{node, activeEnvironment, Stage::AfterArrayCounts});
                    for (auto iterator = arrayCounts.rbegin(); iterator != arrayCounts.rend(); ++iterator)
                        frames.push_back(Frame{*iterator, activeEnvironment, Stage::Enter});
                    continue;
                }
            }
        }

        const LookupEnvironment& scope = *activeEnvironment;
        if (node->throwsTypeSyntax() != nullptr)
        {
            bindTypeSyntax(unit, node->throwsTypeSyntax(), namespaceSegments, scope, diagnostics, index);
        }
        for (const ast::TypeSyntax* syntax : node->typeSyntaxes())
        {
            bindTypeSyntax(unit, syntax, namespaceSegments, scope, diagnostics, index);
        }
        bindConstraintTypeReferences(unit, node, namespaceSegments, scope, diagnostics, index);

        const std::span<ast::AstNode* const> children = node->children();
        for (auto iterator = children.rbegin(); iterator != children.rend(); ++iterator)
            frames.push_back(Frame{*iterator, activeEnvironment, Stage::Enter});
        frames.push_back(Frame{node->third(), activeEnvironment, Stage::Enter});
        frames.push_back(Frame{node->second(), activeEnvironment, Stage::Enter});
        frames.push_back(Frame{node->first(), activeEnvironment, Stage::Enter});
    }
}

void ProjectNameLookup::copyFunctionTypeAlias(
    ast::TypeSyntax& destination,
    const ast::TypeSyntax& source)
{
    destination.setFunction();
    destination.setFunctionCarrier(source.functionCarrier());
    destination.setFunctionCallingConvention(source.functionCallingConvention());
    destination.setFunctionReturnType(source.functionReturnType());
    destination.setFunctionErrorType(source.functionErrorType());
    destination.setFunctionVariadic(source.functionIsVariadic());
    destination.setFunctionUnsafe(source.functionIsUnsafe());
    destination.setFunctionNoHeap(source.functionIsNoHeap());
    destination.setFunctionAsync(source.functionIsAsync());
    destination.setFunctionNullable(source.functionIsNullable());
    destination.setFunctionLifetimeName(source.functionLifetimeName());
    destination.clearTypeArguments();
    for (ast::TypeSyntax* parameter : source.typeArguments())
        destination.addTypeArgument(parameter);
}

void ProjectNameLookup::expandTypeAlias(
    const ast::TypeSyntax* syntax,
    const LookupEnvironment& environment,
    diagnostics::DiagnosticEngine& diagnostics)
{
    // C3 "FFI aliases": expand a type alias INTO this node before anything else looks at it.
    // After this, the AST says what the alias said -- `DWORD` has literally become `uint`,
    // `HANDLE` has become `void*` -- so canonical type keys, object layout, the C ABI rules, HIR
    // and the backend never learn that aliases exist. That is exactly what a typedef means: an
    // alias is not a distinct type, it is another spelling.
    //
    // Chaining falls out of the loop (`using LPDWORD = PDWORD;` where `PDWORD = uint*`), and the
    // loop is bounded so a cycle (`using A = B; using B = A;`) is reported instead of hanging.
    // Modifiers written at the USE site stay outermost: `PDWORD*` is `uint**`, never `uint*`.
    ast::TypeSyntax* mutableSyntax = const_cast<ast::TypeSyntax*>(syntax);
    constexpr int kMaxAliasDepth = 64;
    int depth = 0;
    while (!mutableSyntax->isFunction() &&
           !lexer::isBuiltinTypeKeyword(mutableSyntax->baseTokenKind()) &&
           mutableSyntax->typeArguments().empty())
    {
        const auto alias = environment.typeAliases.find(mutableSyntax->baseName());
        if (alias == environment.typeAliases.end() || alias->second == nullptr) break;
        if (++depth > kMaxAliasDepth)
        {
            diagnostics.report(
                diagnostics::DiagnosticId::InvalidUsingTarget,
                diagnostics::DiagnosticSeverity::Error,
                mutableSyntax->range(),
                "using alias '" + mutableSyntax->baseName() + "' expands cyclically");
            break;
        }
        const ast::TypeSyntax* target = alias->second;
        if (target == mutableSyntax) break;
        mutableSyntax->setBaseTokenKind(target->baseTokenKind());
        mutableSyntax->setBaseName(target->baseName());
        if (target->isFunction()) copyFunctionTypeAlias(*mutableSyntax, *target);
        mutableSyntax->setDynamicTrait(
            mutableSyntax->isDynamicTrait() || target->isDynamicTrait());
        mutableSyntax->setOpaqueResult(
            mutableSyntax->isOpaqueResult() || target->isOpaqueResult());
        if (mutableSyntax->opaqueResultTraitName().empty())
            mutableSyntax->setOpaqueResultTraitName(target->opaqueResultTraitName());
        mutableSyntax->prependModifiers(target->modifiers());
    }
}

void ProjectNameLookup::bindChildTypeSyntaxes(
    const ProjectSyntaxUnit& unit,
    const ast::TypeSyntax* syntax,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& environment,
    diagnostics::DiagnosticEngine& diagnostics,
    UnitBindingIndex& index)
{
    for (const ast::TypeSyntax* argument : syntax->typeArguments())
        bindTypeSyntax(unit, argument, namespaceSegments, environment, diagnostics, index);
    if (!syntax->isFunction()) return;
    bindTypeSyntax(
        unit, syntax->functionReturnType(), namespaceSegments,
        environment, diagnostics, index);
    bindTypeSyntax(
        unit, syntax->functionErrorType(), namespaceSegments,
        environment, diagnostics, index);
}

// NSC-LIMIT-007/009. La escritura COMPLETA de cada tipo contenedor del sitio, de fuera adentro.
// Es la mitad textual de `enclosingTypeOrdinals`, y existe para poder darsela a
// `qualifiedNameLadder` -- que no conoce ordinales a proposito: la fase generica, que usa la misma
// escala, no tiene modelo de nombres del que sacarlos.
std::vector<std::string> ProjectNameLookup::enclosingTypeSpellings(
    const LookupEnvironment& environment) const
{
    std::vector<std::string> spellings;
    spellings.reserve(environment.enclosingTypeOrdinals.size());
    for (const std::size_t enclosing : environment.enclosingTypeOrdinals)
    {
        if (enclosing >= namedTypes_.size()) continue;
        const compilation::TypeIdentity& identity = namedTypes_[enclosing].identity;
        std::string candidate;
        for (const std::string& segment : identity.namespaceSegments())
        {
            if (!candidate.empty()) candidate.push_back('.');
            candidate += segment;
        }
        for (const compilation::TypeNameSegment& segment : identity.containingTypes())
        {
            if (!candidate.empty()) candidate.push_back('.');
            candidate += segment.name;
        }
        if (!candidate.empty()) candidate.push_back('.');
        candidate += identity.name();
        spellings.push_back(std::move(candidate));
    }
    return spellings;
}

std::vector<std::size_t> ProjectNameLookup::resolveExpressionTypeCandidates(
    std::size_t projectOrdinal,
    const std::vector<std::string>& namespaceSegments,
    const LookupEnvironment& environment,
    const std::string& writtenName) const
{
    if (writtenName.find('.') == std::string::npos)
    {
        const auto alias = environment.typeAliases.find(writtenName);
        if (alias != environment.typeAliases.end() && alias->second != nullptr)
        {
            const ast::TypeSyntax* target = alias->second;
            std::unordered_set<std::string> visitedAliases;
            visitedAliases.insert(writtenName);
            while (target != nullptr &&
                   !lexer::isBuiltinTypeKeyword(target->baseTokenKind()) &&
                   target->typeArguments().empty() && target->modifiers().empty())
            {
                const std::string& targetName = target->baseName();
                if (!visitedAliases.insert(targetName).second)
                {
                    return {};
                }

                const auto chainedAlias = environment.typeAliases.find(targetName);
                if (chainedAlias == environment.typeAliases.end())
                {
                    return resolveTypeCandidates(
                        projectOrdinal, namespaceSegments, environment, targetName);
                }
                target = chainedAlias->second;
            }
            return {};
        }
    }
    return resolveTypeCandidates(projectOrdinal, namespaceSegments, environment, writtenName);
}

}
