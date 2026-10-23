#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "../Internal/CloneDepthGuard.h"
#include "../Cache/CachePayloadReader.h"
#include "../Internal/EnumConstResolver.h"
#include "../Internal/SpecializationWaveTimer.h"
#include "../Internal/InterfaceFingerprint.h"
#include "../Internal/ImplementationFingerprint.h"
#include "../Internal/LocatedNode.h"
#include "../Internal/MirFunctionKey.h"
#include "../Internal/MirCacheRecord.h"
#include "../Internal/ProjectGenericDependencyFrame.h"
#include "../Internal/ProjectGenericBodyFrame.h"
#include "../Internal/ProjectGenericAliasFrame.h"
#include "../Internal/ProjectGenericChildWork.h"
#include "../Internal/ProjectGenericCollectFrame.h"
#include "../Internal/ProjectGenericProcessFrame.h"
#include "../Internal/ProjectGenericReservedDemand.h"
#include "../Internal/TypeWork.h"
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
#include "GenericModelSupport.h"
#include "Support/TemplateClassification.h"
#include "Support/TemplateSpelling.h"
#include "Support/ConstGenericArguments.h"
#include "Support/GenericPatterns.h"
#include "Cache/TemplateCatalogCodec.h"
namespace nativesharp::frontend
{
using internal::appendOwner;
using internal::computeTemplateSourceFingerprint;
using internal::constArgumentExpression;
using internal::constGenericParameterTypeKey;
using internal::decodeConstGenericArgument;
using internal::deserializeAstNodeTree;
using internal::enclosingTypeSpellings;
using internal::genericArityAccepts;
using internal::genericAritySpelling;
using internal::genericTemplateAccessibility;
using internal::genericTypeTemplateKind;
using internal::isGenericMethodTemplateNode;
using internal::isGenericTypeTemplateNode;
using internal::isNamedTypeDeclaration;
using internal::isStructuralGenericSurface;
using internal::methodSpecializationFamilyKey;
using internal::normalizeConstExpressionArgument;
using internal::qualifiedName;
using internal::receiverSourceSpelling;
using internal::restoreCatalogEntry;
using internal::serializeAstNodeTree;
using internal::serializeCatalogEntry;
using internal::serializeMirCacheRecord;
using internal::splitDotted;
using internal::stableHashSuffix;
using internal::templateCacheKey;
using internal::templateSourceSpelling;
using internal::trailingPackIndex;
using internal::valueParameterCount;
using internal::CachePayloadReader;
using internal::EnumConstResolver;
using internal::MirCacheRecord;
using internal::SpecializationWaveTimer;
#if 0 // INSTRUMENTATION CODE
using internal::genericNodeDetail;
using internal::genericTypeDetail;
using internal::genericUnitDetail;
using internal::GenericWorkProgress;
#endif // INSTRUMENTATION CODE
void ProjectGenericModel::publishConstEnumType(
    const std::string& spelling,
    const std::string& enumTypeKey)
{
    const auto [entry, inserted] = constEnumTypeBySpelling_.emplace(spelling, enumTypeKey);
    if (!inserted && entry->second != enumTypeKey) entry->second.clear();
}

void ProjectGenericModel::publishConstEnumValue(
    const std::string& spelling,
    const std::pair<std::string, std::string>& identity)
{
    const auto [entry, inserted] = constEnumValueBySpelling_.emplace(spelling, identity);
    if (!inserted && entry->second != identity) entry->second = {};
}

void ProjectGenericModel::collectEnumTemplateData(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::string& currentNamespace)
{
    const std::string qualified = qualifiedName(currentNamespace, node->text());
    const std::string enumTypeKey =
        "enum:" + std::string(unit.moduleName()) + "::" + qualified;
    publishConstEnumType(node->text(), enumTypeKey);
    publishConstEnumType(qualified, enumTypeKey);
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
            const std::optional<ProjectIntegerValue> evaluated =
                evaluator.evaluate(member->first());
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
        publishConstEnumValue(node->text() + "." + member->text(), identity);
        publishConstEnumValue(qualified + "." + member->text(), identity);
        enumResolver.add(member->text(), bits);
        nextValue = bits + 1U;
    }
}

void ProjectGenericModel::collectDeclaredTypeIdentity(
    const ProjectSyntaxUnit& unit,
    const ast::AstNode* node,
    const std::string& currentNamespace,
    const std::string& ownerName)
{
    // NSC-LIMIT-022. Generic specialization happens before ProjectNameLookup exists, but its
    // cache key still has to identify the DECLARATION of an argument, not the project in which
    // that argument happened to be written. Record ordinary named types now, from the same
    // already-parsed graph. Generic declarations are represented by TemplateState and nested
    // declarations inside a generic type are not predeclared by the later name model either.
    const std::string qualifiedTypeName = appendOwner(ownerName, node->text());
    std::string sourceName = qualifiedName(currentNamespace, qualifiedTypeName);
    std::replace(sourceName.begin(), sourceName.end(), '+', '.');
    declaredTypeIdentitiesBySourceName_[sourceName].push_back(declaredTypeIdentities_.size());
    declaredTypeIdentities_.push_back(DeclaredTypeIdentity{
        unit.projectOrdinal(), currentNamespace, std::move(sourceName),
        std::string(unit.moduleName()) + "::" +
            qualifiedName(currentNamespace, qualifiedTypeName),
        node->tokenKind() == lexer::TokenKind::KwPublic});
}

void ProjectGenericModel::collectGenericTypeTemplate(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    ast::AstNode* parent,
    const std::string& currentNamespace,
    const std::string& ownerName,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (node->flag())
    {
        diagnostics.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            node->range(),
            "partial generic types are deferred beyond Stage 44; "
            "a monomorphized template must have one canonical body");
    }
    if (tryRestoreTemplateRecord(unit, node, parent, currentNamespace, ownerName)) return;
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
    templateStates_.push_back(
        TemplateState{record, &unit, node, parent, templateStates_.size(), {}});
}

void ProjectGenericModel::collectGenericMethodTemplate(
    ProjectSyntaxUnit& unit,
    ast::AstNode* node,
    ast::AstNode* parent,
    const std::string& currentNamespace,
    const std::string& ownerName,
    bool insideGenericType,
    diagnostics::DiagnosticEngine& diagnostics)
{
    if (insideGenericType)
    {
        diagnostics.report(
            diagnostics::DiagnosticId::ExpectedDeclaration,
            diagnostics::DiagnosticSeverity::Error,
            node->range(),
            "generic methods declared inside generic types are deliberately deferred beyond "
            "Stage 44; specialize one generic dimension at a time");
    }
    if (tryRestoreTemplateRecord(unit, node, parent, currentNamespace, ownerName)) return;
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
        ? currentNamespace : qualifiedName(currentNamespace, ownerName);
    record.canonicalKey = record.moduleName + "::" +
        (callableScope.empty() ? std::string{} : callableScope + ".") +
        record.name + "``" + genericAritySpelling(record);
    record.sourceFingerprint = computeTemplateSourceFingerprint(unit, node);
    record.c1CacheKey = templateCacheKey(record.sourceFingerprint);
    record.specializationFamilyKey = methodSpecializationFamilyKey(record, node);
    record.isControlledSpecialization =
        node->auxiliaryText() == "controlled-specialization";
    telemetry_.noteTemplate(static_cast<std::size_t>(record.kind));
    templateStates_.push_back(
        TemplateState{record, &unit, node, parent, templateStates_.size(), {}});
}

void ProjectGenericModel::pushCollectTemplateChildren(
    ast::AstNode* node,
    const std::string& currentNamespace,
    const std::string& ownerName,
    bool insideGenericType,
    bool genericType,
    std::vector<CollectFrame>& stack)
{
    // NSC-LIMIT-009: se APILA. La version anterior reemplazaba, de modo que al entrar en
    // `A { Inner { ... } }` el dueno pasaba a ser «Inner» y `A` desaparecia.
    const std::string childOwner =
        (node->kind() == ast::AstNodeKind::ClassDeclaration ||
         node->kind() == ast::AstNodeKind::StructDeclaration)
            ? appendOwner(ownerName, node->text()) : ownerName;
    const bool childInsideGeneric = insideGenericType || genericType;
    const std::vector<ast::AstNode*> children(node->children().begin(), node->children().end());
    for (auto iterator = children.rbegin(); iterator != children.rend(); ++iterator)
        stack.push_back(CollectFrame{
            *iterator, node, currentNamespace, childOwner, childInsideGeneric});
}

void ProjectGenericModel::processCollectTemplateFrame(
    ProjectSyntaxUnit& unit,
    CollectFrame frame,
    std::vector<CollectFrame>& stack,
    diagnostics::DiagnosticEngine& diagnostics)
{
    ast::AstNode* node = frame.node;
    if (node == nullptr) return;
    std::string currentNamespace = std::move(frame.namespaceName);
    if (node->kind() == ast::AstNodeKind::NamespaceDeclaration)
        currentNamespace = namespaceAppend(currentNamespace, node->text());
    if (node->kind() == ast::AstNodeKind::ClassDeclaration &&
        node->auxiliaryText() == "ns3-trait-impl")
        return;
    if (!frame.insideGenericType && node->kind() == ast::AstNodeKind::EnumDeclaration)
        collectEnumTemplateData(unit, node, currentNamespace);
    if (!frame.insideGenericType && isNamedTypeDeclaration(node->kind()) &&
        node->genericParameters().empty())
        collectDeclaredTypeIdentity(unit, node, currentNamespace, frame.ownerName);
    const bool genericType = isGenericTypeTemplateNode(node);
    if (genericType)
        collectGenericTypeTemplate(
            unit, node, frame.parent, currentNamespace, frame.ownerName, diagnostics);
    if (isGenericMethodTemplateNode(node))
        collectGenericMethodTemplate(
            unit, node, frame.parent, currentNamespace, frame.ownerName,
            frame.insideGenericType, diagnostics);
    pushCollectTemplateChildren(
        node, currentNamespace, frame.ownerName, frame.insideGenericType,
        genericType, stack);
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
    std::vector<CollectFrame> stack;
    stack.push_back(CollectFrame{
        node, parent, std::move(namespaceName), std::move(ownerName), insideGenericType});
    while (!stack.empty())
    {
        CollectFrame frame = std::move(stack.back());
        stack.pop_back();
        processCollectTemplateFrame(unit, std::move(frame), stack, diagnostics);
    }
}
}
