#include "nativesharp/LLVM/ProjectOopFunctionReachability.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include <algorithm>
#include <span>
#include <string_view>

namespace nativesharp::llvm_backend
{
namespace
{
void appendIdentityField(std::string& output, std::string_view value)
{
    output += std::to_string(value.size());
    output.push_back(':');
    output.append(value);
    output.push_back(';');
}
}

ProjectOopFunctionReachabilityAnalysis::ProjectOopFunctionReachabilityAnalysis(
    const frontend::ProjectOopMirModel& mirModel)
    : mirModel_(mirModel),
      reachableDefinitions_(mirModel.functions().size(), 0U)
{
    const std::span<const frontend::ProjectOopMirFunction> functions = mirModel.functions();
    functionsByName_.reserve(functions.size());
    functionByMember_.reserve(functions.size());
    std::string identity = "NativeSharp.ProjectOopLlvmAnalysis.v1;";
    identity += std::to_string(functions.size()) + ';';
    for (std::size_t ordinal = 0; ordinal < functions.size(); ++ordinal)
    {
        const frontend::ProjectOopMirFunction& function = functions[ordinal];
        functionsByName_[function.mangledName].push_back(ordinal);
        if (!function.isLambda && !function.namespaceFunctionOrdinal.has_value())
            functionByMember_.emplace(function.memberOrdinal, ordinal);
        identity += std::to_string(ordinal) + ';';
        appendIdentityField(identity, function.mangledName);
        identity.push_back(function.isImportedC ? '1' : '0');
        identity.push_back(function.isImportedNative ? '1' : '0');
        identity.push_back(function.isTransportedInlineBody ? '1' : '0');
    }

    const std::span<const frontend::ProjectOopMirStaticField> fields = mirModel.staticFields();
    staticFieldByMember_.reserve(fields.size());
    identity += std::to_string(fields.size()) + ';';
    for (std::size_t ordinal = 0; ordinal < fields.size(); ++ordinal)
    {
        staticFieldByMember_.emplace(fields[ordinal].memberOrdinal, ordinal);
        appendIdentityField(identity, fields[ordinal].storageSymbol);
    }

    std::vector<std::size_t> work;
    work.reserve(functions.size());
    for (std::size_t ordinal = 0; ordinal < functions.size(); ++ordinal)
        if (!functions[ordinal].isTransportedInlineBody)
            addReachableFunction(ordinal, work);
    while (!work.empty())
    {
        const std::size_t ordinal = work.back();
        work.pop_back();
        const frontend::ProjectOopMirFunction& function = functions[ordinal];
        scanReachableFunctionReferences(function.body.get(), work);
        for (const frontend::ProjectOopMirCleanupRegion& cleanup : function.cleanupRegions)
            scanReachableFunctionReferences(cleanup.action, work);
    }

    const auto bytes = std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(identity.data()), identity.size());
    projectFingerprint_ = basic::sha256Hex(basic::sha256(bytes));
}

void ProjectOopFunctionReachabilityAnalysis::addReachableFunction(
    std::size_t ordinal, std::vector<std::size_t>& work)
{
    const std::span<const frontend::ProjectOopMirFunction> functions = mirModel_.functions();
    if (ordinal >= functions.size() || reachableDefinitions_[ordinal] != 0U ||
        functions[ordinal].isImportedC || functions[ordinal].isImportedNative)
        return;
    reachableDefinitions_[ordinal] = 1U;
    work.push_back(ordinal);
}

void ProjectOopFunctionReachabilityAnalysis::addReachableFunctionName(
    std::string_view name, std::vector<std::size_t>& work)
{
    if (name.empty()) return;
    const auto found = functionsByName_.find(name);
    if (found == functionsByName_.end()) return;
    for (const std::size_t ordinal : found->second) addReachableFunction(ordinal, work);
}

void ProjectOopFunctionReachabilityAnalysis::addReachableFunctionMember(
    std::size_t member, std::vector<std::size_t>& work)
{
    const auto found = functionByMember_.find(member);
    if (found != functionByMember_.end()) addReachableFunction(found->second, work);
}

void ProjectOopFunctionReachabilityAnalysis::scanReachableFunctionReferences(
    const frontend::ProjectOopMirNode* root, std::vector<std::size_t>& work)
{
    std::vector<const frontend::ProjectOopMirNode*> nodes;
    if (root != nullptr) nodes.push_back(root);
    while (!nodes.empty())
    {
        const frontend::ProjectOopMirNode* node = nodes.back();
        nodes.pop_back();
        if (node == nullptr) continue;
        addReachableFunctionName(node->calleeName, work);
        addReachableFunctionName(node->secondaryCalleeName, work);
        addReachableFunctionName(node->resultSuccessTestCalleeName, work);
        addReachableFunctionName(node->resultSuccessExtractorCalleeName, work);
        addReachableFunctionName(node->resultErrorExtractorCalleeName, work);
        addReachableFunctionName(node->resultErrorFactoryCalleeName, work);
        addReachableFunctionName(node->resultErrorConversionCalleeName, work);
        if (node->kind == frontend::ProjectOopMirNodeKind::PollAddress)
            addReachableFunctionName(node->stringValue, work);
        if (node->kind == frontend::ProjectOopMirNodeKind::Panic)
            addReachableFunctionName(mirModel_.panicHandlerMangledName(), work);
        if (node->memberOrdinal.has_value())
            addReachableFunctionMember(*node->memberOrdinal, work);
        if (node->secondaryMemberOrdinal.has_value())
            addReachableFunctionMember(*node->secondaryMemberOrdinal, work);
        if (node->resultSuccessTestMemberOrdinal.has_value())
            addReachableFunctionMember(*node->resultSuccessTestMemberOrdinal, work);
        if (node->resultSuccessExtractorMemberOrdinal.has_value())
            addReachableFunctionMember(*node->resultSuccessExtractorMemberOrdinal, work);
        if (node->resultErrorExtractorMemberOrdinal.has_value())
            addReachableFunctionMember(*node->resultErrorExtractorMemberOrdinal, work);
        if (node->resultErrorFactoryMemberOrdinal.has_value())
            addReachableFunctionMember(*node->resultErrorFactoryMemberOrdinal, work);
        if (node->resultErrorConversionMemberOrdinal.has_value())
            addReachableFunctionMember(*node->resultErrorConversionMemberOrdinal, work);
        if (node->disposeImplementationMemberOrdinal.has_value())
            addReachableFunctionMember(*node->disposeImplementationMemberOrdinal, work);
        if (node->allocatorDeallocateMemberOrdinal.has_value())
            addReachableFunctionMember(*node->allocatorDeallocateMemberOrdinal, work);
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            if (child->get() != nullptr) nodes.push_back(child->get());
    }
}

std::span<const std::uint8_t>
ProjectOopFunctionReachabilityAnalysis::reachableDefinitions() const noexcept
{
    return reachableDefinitions_;
}

void ProjectOopFunctionReachabilityAnalysis::addFunctionName(
    std::string_view name, std::vector<std::uint8_t>& selected) const
{
    if (name.empty()) return;
    const auto found = functionsByName_.find(name);
    if (found == functionsByName_.end()) return;
    for (const std::size_t ordinal : found->second)
        if (ordinal < selected.size()) selected[ordinal] = 1U;
}

void ProjectOopFunctionReachabilityAnalysis::addFunctionMember(
    std::size_t member, std::vector<std::uint8_t>& selected) const
{
    const auto found = functionByMember_.find(member);
    if (found != functionByMember_.end() && found->second < selected.size())
        selected[found->second] = 1U;
}

void ProjectOopFunctionReachabilityAnalysis::addStaticFieldMember(
    std::size_t member, std::vector<std::uint8_t>& selected) const
{
    const auto found = staticFieldByMember_.find(member);
    if (found != staticFieldByMember_.end() && found->second < selected.size())
        selected[found->second] = 1U;
}

void ProjectOopFunctionReachabilityAnalysis::scanFunctionReferences(
    const frontend::ProjectOopMirModel& mirModel,
    const frontend::ProjectOopMirNode* root,
    std::vector<std::uint8_t>& selected) const
{
    std::vector<const frontend::ProjectOopMirNode*> work;
    if (root != nullptr) work.push_back(root);
    while (!work.empty())
    {
        const frontend::ProjectOopMirNode* node = work.back();
        work.pop_back();
        if (node == nullptr) continue;
        addFunctionName(node->calleeName, selected);
        addFunctionName(node->secondaryCalleeName, selected);
        addFunctionName(node->resultSuccessTestCalleeName, selected);
        addFunctionName(node->resultSuccessExtractorCalleeName, selected);
        addFunctionName(node->resultErrorExtractorCalleeName, selected);
        addFunctionName(node->resultErrorFactoryCalleeName, selected);
        addFunctionName(node->resultErrorConversionCalleeName, selected);
        if (node->kind == frontend::ProjectOopMirNodeKind::PollAddress)
            addFunctionName(node->stringValue, selected);
        if (node->kind == frontend::ProjectOopMirNodeKind::Panic)
            addFunctionName(mirModel.panicHandlerMangledName(), selected);
        if (node->memberOrdinal.has_value()) addFunctionMember(*node->memberOrdinal, selected);
        if (node->secondaryMemberOrdinal.has_value())
            addFunctionMember(*node->secondaryMemberOrdinal, selected);
        if (node->resultSuccessTestMemberOrdinal.has_value())
            addFunctionMember(*node->resultSuccessTestMemberOrdinal, selected);
        if (node->resultSuccessExtractorMemberOrdinal.has_value())
            addFunctionMember(*node->resultSuccessExtractorMemberOrdinal, selected);
        if (node->resultErrorExtractorMemberOrdinal.has_value())
            addFunctionMember(*node->resultErrorExtractorMemberOrdinal, selected);
        if (node->resultErrorFactoryMemberOrdinal.has_value())
            addFunctionMember(*node->resultErrorFactoryMemberOrdinal, selected);
        if (node->resultErrorConversionMemberOrdinal.has_value())
            addFunctionMember(*node->resultErrorConversionMemberOrdinal, selected);
        if (node->disposeContractMemberOrdinal.has_value())
            addFunctionMember(*node->disposeContractMemberOrdinal, selected);
        if (node->disposeImplementationMemberOrdinal.has_value())
            addFunctionMember(*node->disposeImplementationMemberOrdinal, selected);
        if (node->allocatorDeallocateMemberOrdinal.has_value())
            addFunctionMember(*node->allocatorDeallocateMemberOrdinal, selected);
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            if (child->get() != nullptr) work.push_back(child->get());
    }
}

void ProjectOopFunctionReachabilityAnalysis::scanStaticFieldReferences(
    const frontend::ProjectOopMirNode* root,
    std::vector<std::uint8_t>& selected) const
{
    std::vector<const frontend::ProjectOopMirNode*> work;
    if (root != nullptr) work.push_back(root);
    while (!work.empty())
    {
        const frontend::ProjectOopMirNode* node = work.back();
        work.pop_back();
        if (node == nullptr) continue;
        if (node->memberOrdinal.has_value()) addStaticFieldMember(*node->memberOrdinal, selected);
        if (node->secondaryMemberOrdinal.has_value())
            addStaticFieldMember(*node->secondaryMemberOrdinal, selected);
        if (node->resultSuccessTestMemberOrdinal.has_value())
            addStaticFieldMember(*node->resultSuccessTestMemberOrdinal, selected);
        if (node->resultSuccessExtractorMemberOrdinal.has_value())
            addStaticFieldMember(*node->resultSuccessExtractorMemberOrdinal, selected);
        if (node->resultErrorExtractorMemberOrdinal.has_value())
            addStaticFieldMember(*node->resultErrorExtractorMemberOrdinal, selected);
        if (node->resultErrorFactoryMemberOrdinal.has_value())
            addStaticFieldMember(*node->resultErrorFactoryMemberOrdinal, selected);
        if (node->resultErrorConversionMemberOrdinal.has_value())
            addStaticFieldMember(*node->resultErrorConversionMemberOrdinal, selected);
        if (node->disposeContractMemberOrdinal.has_value())
            addStaticFieldMember(*node->disposeContractMemberOrdinal, selected);
        if (node->disposeImplementationMemberOrdinal.has_value())
            addStaticFieldMember(*node->disposeImplementationMemberOrdinal, selected);
        if (node->allocatorDeallocateMemberOrdinal.has_value())
            addStaticFieldMember(*node->allocatorDeallocateMemberOrdinal, selected);
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            if (child->get() != nullptr) work.push_back(child->get());
    }
}

std::vector<std::size_t>
ProjectOopFunctionReachabilityAnalysis::coordinatorFunctionDeclarations() const
{
    const std::span<const frontend::ProjectOopMirFunction> functions = mirModel_.functions();
    std::vector<std::uint8_t> selected(functions.size(), 0U);
    for (std::size_t ordinal = 0; ordinal < functions.size(); ++ordinal)
    {
        const frontend::ProjectOopMirFunction& function = functions[ordinal];
        if (!function.isImportedC && !function.isImportedNative &&
            ordinal < reachableDefinitions_.size() && reachableDefinitions_[ordinal] != 0U)
            selected[ordinal] = 1U;
    }

    const std::span<const frontend::ProjectOopMirStaticField> fields = mirModel_.staticFields();
    for (const frontend::ProjectOopMirFieldInitializer& initializer : mirModel_.fieldInitializers())
    {
        const auto field = staticFieldByMember_.find(initializer.memberOrdinal);
        if (field == staticFieldByMember_.end() || field->second >= fields.size() ||
            fields[field->second].isImported)
            continue;
        scanFunctionReferences(mirModel_, initializer.value.get(), selected);
    }
    for (const frontend::ProjectOopMirDispatchTable& table : mirModel_.dispatchTables())
    {
        if (table.isImported) continue;
        for (const std::string& callee : table.slotCalleeNames)
            addFunctionName(callee, selected);
        for (const std::string& callee : table.destructorCalleeNames)
            addFunctionName(callee, selected);
        for (const frontend::ProjectOopMirDispatchTable::InterfaceTable& interfaceTable :
             table.interfaceTables)
            for (const std::string& callee : interfaceTable.calleeNames)
                addFunctionName(callee, selected);
    }

    std::vector<std::size_t> ordinals;
    ordinals.reserve(functions.size());
    for (std::size_t ordinal = 0; ordinal < selected.size(); ++ordinal)
        if (selected[ordinal] != 0U) ordinals.push_back(ordinal);
    return ordinals;
}

std::vector<std::uint8_t>
ProjectOopFunctionReachabilityAnalysis::staticFieldDeclarationMask(
    std::span<const std::size_t> definitionOrdinals,
    bool partitioned,
    bool ownsModuleGlobals) const
{
    const std::span<const frontend::ProjectOopMirStaticField> fields = mirModel_.staticFields();
    std::vector<std::uint8_t> selected(fields.size(), 0U);
    if (!partitioned)
    {
        std::fill(selected.begin(), selected.end(), 1U);
        return selected;
    }

    if (ownsModuleGlobals)
    {
        for (std::size_t ordinal = 0; ordinal < fields.size(); ++ordinal)
            if (!fields[ordinal].isImported || fields[ordinal].isConst) selected[ordinal] = 1U;
        for (const frontend::ProjectOopMirFieldInitializer& initializer : mirModel_.fieldInitializers())
        {
            const auto target = staticFieldByMember_.find(initializer.memberOrdinal);
            if (target == staticFieldByMember_.end() || target->second >= fields.size() ||
                fields[target->second].isImported)
                continue;
            selected[target->second] = 1U;
            scanStaticFieldReferences(initializer.value.get(), selected);
        }
        return selected;
    }

    const std::span<const frontend::ProjectOopMirFunction> functions = mirModel_.functions();
    for (const std::size_t ordinal : definitionOrdinals)
    {
        if (ordinal >= functions.size()) continue;
        const frontend::ProjectOopMirFunction& function = functions[ordinal];
        scanStaticFieldReferences(function.body.get(), selected);
        for (const frontend::ProjectOopMirCleanupRegion& cleanup : function.cleanupRegions)
            scanStaticFieldReferences(cleanup.action, selected);
        for (const frontend::ProjectOopMirNonlocalExit& exit : function.exceptionFlow.nonlocalExits)
            scanStaticFieldReferences(exit.node, selected);
    }
    return selected;
}

const std::string& ProjectOopFunctionReachabilityAnalysis::projectFingerprint() const noexcept
{
    return projectFingerprint_;
}
}
