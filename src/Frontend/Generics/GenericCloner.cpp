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
namespace
{
constexpr std::string_view kGenericRawNewAuxiliary = "stage52-generic-raw-new";
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
using internal::CloneDepthGuard;
}  // namespace

// NSC305-G06. The non-substitution branch of cloneType (see the header).
// NSC305-G06. The shallow (non-recursive) part of the generic type clone (see the header).
ast::TypeSyntax* ProjectGenericModel::cloneTypeGenericShallow(
    ast::AstArena& arena,
    const ast::TypeSyntax* source)
{
    ast::TypeSyntax* result =
        arena.make<ast::TypeSyntax>(source->range(), source->baseTokenKind(), source->baseName());
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
    // Stage 52 fix: carry forward the origin tags the SOURCE node already carries.
    if (source->originProjectOrdinal().has_value())
    {
        result->setOriginProjectOrdinal(source->originProjectOrdinal().value());
    }
    if (source->originNamespace().has_value())
    {
        result->setOriginNamespace(source->originNamespace().value());
    }
    if (const DeclaredTypeOrigin* origin = lookupResolvedTypeOrigin(source); origin != nullptr)
    {
        recordResolvedTypeOrigin(result, *origin);
    }
    return result;
}

ast::TypeSyntax* ProjectGenericModel::cloneProjectionResult(
    ast::AstArena& arena,
    const ast::TypeSyntax* source,
    const ast::TypeSyntax* current,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    std::size_t modifierLimit)
{
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

ast::TypeSyntax* ProjectGenericModel::recoverAssociatedProjectionFailure(
    ast::AstArena& arena,
    const ast::TypeSyntax* source,
    bool cycle,
    std::string_view detail)
{
    if (associatedProjectionError_.empty())
    {
        associatedProjectionError_ = cycle
            ? "associated type normalization cycle reaches '" + std::string(detail) + "'"
            : "cannot normalize associated type projection '" + source->baseName() +
                "' for concrete type '" + std::string(detail) +
                "': no associated type definition is available";
        associatedProjectionErrorRange_ = source->range();
    }
    return arena.make<ast::TypeSyntax>(
        source->range(), lexer::TokenKind::KwInt, "int");
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
            if (const std::string* identity = lookupResolvedTypeIdentity(current);
                identity != nullptr)
            {
                const std::size_t moduleSeparator = identity->find("::");
                if (moduleSeparator != std::string::npos)
                    candidates.push_back(identity->substr(moduleSeparator + 2U) +
                        "." + item);
            }
        }
        if (current != nullptr && current->originNamespace().has_value() &&
            !current->originNamespace()->empty())
            candidates.push_back(namespaceAppend(
                *current->originNamespace(), owner) + "." + item);
        const std::string_view instantiationNamespace = workerSpecializationContext_ != nullptr
            ? std::string_view(workerSpecializationContext_->namespaceName)
            : std::string_view(currentInstantiationNamespace_);
        if (!instantiationNamespace.empty())
            candidates.push_back(namespaceAppend(
                instantiationNamespace, owner) + "." + item);

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
            return recoverAssociatedProjectionFailure(arena, source, false, owner);
        }
        if (!activeAssociatedProjections_.insert(selectedKey).second)
        {
            return recoverAssociatedProjectionFailure(
                arena, source, true, selectedKey);
        }
        current = cloneType(arena, definition, substitutions);
        activeAssociatedProjections_.erase(selectedKey);
        if (end == std::string::npos) break;
        begin = end + 1U;
    }

    return cloneProjectionResult(arena, source, current, substitutions, modifierLimit);
}

bool ProjectGenericModel::isSpecialCloneType(
    const ast::TypeSyntax* syntax,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions) const
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
}

void ProjectGenericModel::seedCloneTypeWork(
    const ast::TypeSyntax* source,
    ast::TypeSyntax* result,
    std::vector<TypeWork>& work) const
{
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
}

ast::TypeSyntax* ProjectGenericModel::cloneTypeWorkItem(
    ast::AstArena& arena,
    const TypeWork& item,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    std::size_t modifierLimit,
    std::vector<TypeWork>& work)
{
    if (item.source == nullptr) return nullptr;
    if (item.slot == 2 && !item.parent->isTuple() && item.source->isPackExpansion())
    {
        for (ast::TypeSyntax* expanded :
             cloneExpandedTypeArgument(arena, item.source, substitutions))
            item.parent->addTypeArgument(expanded);
        return nullptr;
    }
    if (isSpecialCloneType(item.source, substitutions))
        return cloneType(arena, item.source, substitutions);

    ast::TypeSyntax* child = cloneTypeGenericShallow(arena, item.source);
    if (item.source->constExpression() != nullptr)
        child->setConstExpression(cloneNode(
            arena, item.source->constExpression(), substitutions, "", ""));
    // simple modifiers (no substitution identity handling)
    const std::size_t count = std::min(item.source->modifiers().size(), modifierLimit);
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
    return child;
}

void ProjectGenericModel::attachClonedType(const TypeWork& item, ast::TypeSyntax* child)
{
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

ast::TypeSyntax* ProjectGenericModel::cloneTypeGeneric(
    ast::AstArena& arena,
    const ast::TypeSyntax* source,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    std::size_t modifierLimit)
{
    ast::TypeSyntax* result = cloneTypeGenericShallow(arena, source);
    if (source->constExpression() != nullptr)
        result->setConstExpression(cloneNode(
            arena, source->constExpression(), substitutions, "", ""));
    // NSC305-G06. Iteratively clone the return/error/argument types with an explicit stack.
    // Simple generic types are shallow-cloned here and their sub-types pushed; projection, pack,
    // substitution, tuple and pack-expansion types delegate back to cloneType (bounded/rare).
    std::vector<TypeWork> work;
    seedCloneTypeWork(source, result, work);
    while (!work.empty())
    {
        TypeWork item = work.back();
        work.pop_back();
        ast::TypeSyntax* child =
            cloneTypeWorkItem(arena, item, substitutions, modifierLimit, work);
        if (child != nullptr) attachClonedType(item, child);
    }
    return result;
}

void ProjectGenericModel::appendCloneModifiers(
    ast::AstArena& arena,
    const ast::TypeSyntax* source,
    ast::TypeSyntax* result,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    std::size_t modifierLimit,
    bool updateIdentity)
{
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
        if (!updateIdentity) continue;
        if (std::string* identity = mutableResolvedTypeIdentity(result); identity != nullptr)
        {
            if (modifier.kind == ast::TypeModifierSyntaxKind::Pointer)
                identity->push_back('*');
            else if (modifier.kind == ast::TypeModifierSyntaxKind::DynamicArray)
                *identity += "[]";
            else if (modifier.kind == ast::TypeModifierSyntaxKind::RangeArray)
                *identity += '[' + std::string(
                    modifier.arrayRank > 0 ? modifier.arrayRank - 1 : 0, ',') + ']';
            else if (modifier.kind == ast::TypeModifierSyntaxKind::FlexibleArray)
                *identity += "[*]";
            else
                *identity += "[fixed]";
        }
    }
}

ast::TypeSyntax* ProjectGenericModel::cloneSubstitutedOrGenericType(
    ast::AstArena& arena,
    const ast::TypeSyntax* source,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    std::size_t modifierLimit,
    bool& substituted)
{
    const auto substitution = substitutions.find(source->baseName());
    if (substitution != substitutions.end() && source->typeArguments().empty())
    {
        substituted = true;
        ast::TypeSyntax* result = cloneType(arena, substitution->second, {});
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
        return result;
    }
    substituted = false;
    return cloneTypeGeneric(arena, source, substitutions, modifierLimit);
}

ast::TypeSyntax* ProjectGenericModel::clonePackType(
    ast::AstArena& arena,
    const ast::TypeSyntax* source,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    std::size_t modifierLimit)
{
    if (source->baseName() != "packtype" || source->typeArguments().size() != 2U)
        return nullptr;
    const ast::TypeSyntax* packSyntax = source->typeArguments()[0];
    const auto pack = packSyntax != nullptr && packSyntax->typeArguments().empty()
        ? activePackSubstitutions_.find(packSyntax->baseName())
        : activePackSubstitutions_.end();
    if (pack == activePackSubstitutions_.end()) return nullptr;

    std::string indexType;
    std::string indexValue;
    std::size_t index = 0;
    const bool decodedIndex = decodeConstGenericArgument(
        source->typeArguments()[1], indexType, indexValue);
    const auto parsedIndex = decodedIndex && indexValue.find('-') == std::string::npos
        ? std::from_chars(indexValue.data(), indexValue.data() + indexValue.size(), index)
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
                    " is outside a pack of length " + std::to_string(pack->second.size())
                : "generic pack type index must be a non-negative compile-time integer";
            packMaterializationErrorRange_ = source->range();
        }
        return arena.make<ast::TypeSyntax>(
            source->range(), lexer::TokenKind::KwInt, "int");
    }
    ast::TypeSyntax* selected = cloneType(arena, pack->second[index], substitutions);
    appendCloneModifiers(arena, source, selected, substitutions, modifierLimit, false);
    selected->setOwnershipQualifier(source->ownershipQualifier());
    selected->setLifetimeName(source->lifetimeName());
    selected->setDynamicTrait(source->isDynamicTrait());
    selected->setOpaqueResult(source->isOpaqueResult());
    selected->setOpaqueResultTraitName(source->opaqueResultTraitName());
    selected->setExplicitTraitImplementation(source->isExplicitTraitImplementation());
    selected->setPointeeConst(source->isPointeeConst());
    selected->setRange(source->range());
    return selected;
}

void ProjectGenericModel::applyClonedTypeSurface(
    const ast::TypeSyntax* source,
    ast::TypeSyntax* result,
    bool substituted)
{
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
        if (const std::string* identity = lookupResolvedTypeIdentity(source); identity != nullptr)
        {
            recordResolvedTypeIdentity(result, *identity);
        }
    }
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
    if (ast::TypeSyntax* pack = clonePackType(arena, source, substitutions, modifierLimit))
        return pack;

    bool substituted = false;
    ast::TypeSyntax* result = cloneSubstitutedOrGenericType(
        arena, source, substitutions, modifierLimit, substituted);
    appendCloneModifiers(
        arena, source, result, substitutions, modifierLimit, substituted);
    applyClonedTypeSurface(source, result, substituted);
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
    if (const DeclaredTypeOrigin* origin = lookupResolvedTypeOrigin(syntax); origin != nullptr)
    {
        syntax->setOriginProjectOrdinal(origin->projectOrdinal);
        syntax->setOriginNamespace(origin->namespaceName);
    }
    else
    {
        const std::size_t projectOrdinal = workerSpecializationContext_ != nullptr
            ? workerSpecializationContext_->projectOrdinal
            : currentInstantiationProjectOrdinal_;
        const std::string& namespaceName = workerSpecializationContext_ != nullptr
            ? workerSpecializationContext_->namespaceName
            : currentInstantiationNamespace_;
        syntax->setOriginProjectOrdinal(projectOrdinal);
        syntax->setOriginNamespace(namespaceName);
    }
}

ast::AstNode* ProjectGenericModel::foldValuePackAt(
    ast::AstArena& arena,
    const ast::AstNode* source,
    const std::vector<const ast::TypeSyntax*>& pack,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    std::string_view rewrittenContainingTypeName,
    std::string_view originalContainingTypeName)
{
    ast::AstNode* clonedIndex = cloneNode(
        arena, source->children()[1], substitutions,
        rewrittenContainingTypeName, originalContainingTypeName);
    const ProjectIntegerEvaluator evaluator(nullptr);
    const std::optional<ProjectIntegerValue> evaluated = evaluator.evaluate(clonedIndex);
    const bool validIndex = evaluated.has_value() && evaluated->asSigned() >= 0;
    const std::size_t index = validIndex
        ? static_cast<std::size_t>(evaluated->bits) : 0U;
    if (validIndex && index < pack.size())
    {
        std::string typeKey;
        std::string value;
        if (decodeConstGenericArgument(pack[index], typeKey, value))
            if (ast::AstNode* selected = constArgumentExpression(
                    arena, typeKey, value, source->range()))
                return selected;
    }
    if (packMaterializationError_.empty())
    {
        packMaterializationError_ = !validIndex
            ? "generic value-pack index must be a non-negative compile-time integer"
            : index >= pack.size()
                ? "generic value-pack index " + std::to_string(index) +
                    " is outside a pack of length " + std::to_string(pack.size())
                : "packat requires a const value pack";
        packMaterializationErrorRange_ = source->range();
    }
    return constArgumentExpression(arena, "builtin:int", "0", source->range());
}

ast::AstNode* ProjectGenericModel::foldValuePackReduction(
    ast::AstArena& arena,
    const ast::AstNode* source,
    const std::string& operation,
    const std::vector<const ast::TypeSyntax*>& pack)
{
    const bool arithmeticFold = operation == "packsum" || operation == "packproduct";
    const bool booleanFold = operation == "packall" || operation == "packany";
    if (!arithmeticFold && !booleanFold) return nullptr;
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
    for (const ast::TypeSyntax* item : pack)
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
        return foldValuePackAt(
            arena, source, pack->second, substitutions,
            rewrittenContainingTypeName, originalContainingTypeName);
    }

    if (source->children().size() == 1U)
        return foldValuePackReduction(arena, source, operation, pack->second);
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

bool ProjectGenericModel::isLayoutQueryOfSubstitutedParameter(
    const ast::AstNode* node,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions) const
{
    return node != nullptr &&
        (node->kind() == ast::AstNodeKind::SizeofExpression ||
         node->kind() == ast::AstNodeKind::AlignofExpression) &&
        node->first() != nullptr &&
        node->first()->kind() == ast::AstNodeKind::IdentifierExpression &&
        substitutions.contains(node->first()->text());
}

void ProjectGenericModel::cloneGenericConstraints(
    ast::AstArena& arena,
    const ast::AstNode* source,
    ast::AstNode* result,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions)
{
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
        copy.enclosingTypeParameter = clause.enclosingTypeParameter;
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
        else if (clause.enclosingTypeParameter && clause.substitutedArgument != nullptr)
        {
            // EN: Freeze 3.0 P1: a member clause keeps the argument of its closed type when the
            //     member is cloned again without that parameter.
            // ES: Congelamiento 3.0 P1: una clausula de miembro conserva el argumento de su tipo
            //     cerrado cuando el miembro se vuelve a clonar sin ese parametro.
            copy.substitutedArgument = cloneType(arena, clause.substitutedArgument, substitutions);
        }
        result->addGenericConstraint(std::move(copy));
    }
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
        isLayoutQueryOfSubstitutedParameter(source, substitutions);
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
    cloneGenericConstraints(arena, source, result, substitutions);
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

void ProjectGenericModel::cloneNodeChildrenIterative(
    ast::AstArena& arena,
    const ast::AstNode* source,
    ast::AstNode* result,
    const std::unordered_map<std::string, const ast::TypeSyntax*>& substitutions,
    std::string_view rewrittenContainingTypeName,
    std::string_view originalContainingTypeName)
{
    std::vector<ChildWork> work;
    for (auto iterator = source->children().rbegin();
         iterator != source->children().rend(); ++iterator)
        work.push_back(ChildWork{*iterator, result, 4});
    if (source->third() != nullptr)
        work.push_back(ChildWork{source->third(), result, 3});
    if (source->second() != nullptr)
        work.push_back(ChildWork{source->second(), result, 2});
    if (!isLayoutQueryOfSubstitutedParameter(source, substitutions))
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
                for (auto iterator = item.source->children().rbegin();
                     iterator != item.source->children().rend(); ++iterator)
                    work.push_back(ChildWork{*iterator, child, 4});
                if (item.source->third() != nullptr)
                    work.push_back(ChildWork{item.source->third(), child, 3});
                if (item.source->second() != nullptr)
                    work.push_back(ChildWork{item.source->second(), child, 2});
                if (!isLayoutQueryOfSubstitutedParameter(item.source, substitutions))
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
    ast::AstNode* result = cloneNodeGeneric(
        arena, source, substitutions, rewrittenContainingTypeName, originalContainingTypeName);
    // NSC305-G06. Iteratively clone the child subtree with an explicit stack. Special cases are
    // handled by tryCloneSpecialNode; generic children are shallow-cloned here and their own
    // children pushed, so deeply nested generic expressions no longer recurse.
    cloneNodeChildrenIterative(
        arena, source, result, substitutions,
        rewrittenContainingTypeName, originalContainingTypeName);
    return result;
}
}
