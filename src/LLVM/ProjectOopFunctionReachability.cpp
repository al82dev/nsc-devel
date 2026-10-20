#include "nativesharp/LLVM/ProjectOopFunctionReachability.h"

#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include <algorithm>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>

namespace nativesharp::llvm_backend
{
namespace
{
template <typename AddName, typename AddMember>
void scanFunctionReferences(
    const frontend::ProjectOopMirNode* root,
    const frontend::ProjectOopMirModel& mirModel,
    AddName&& addName,
    AddMember&& addMember,
    bool includeDisposeContract)
{
    std::vector<const frontend::ProjectOopMirNode*> nodes;
    if (root != nullptr) nodes.push_back(root);
    while (!nodes.empty())
    {
        const frontend::ProjectOopMirNode* node = nodes.back();
        nodes.pop_back();
        if (node == nullptr) continue;
        addName(node->calleeName);
        addName(node->secondaryCalleeName);
        addName(node->resultSuccessTestCalleeName);
        addName(node->resultSuccessExtractorCalleeName);
        addName(node->resultErrorExtractorCalleeName);
        addName(node->resultErrorFactoryCalleeName);
        addName(node->resultErrorConversionCalleeName);
        if (node->kind == frontend::ProjectOopMirNodeKind::PollAddress)
            addName(node->stringValue);
        if (node->kind == frontend::ProjectOopMirNodeKind::Panic)
            addName(mirModel.panicHandlerMangledName());
        addMember(node->memberOrdinal);
        addMember(node->secondaryMemberOrdinal);
        addMember(node->resultSuccessTestMemberOrdinal);
        addMember(node->resultSuccessExtractorMemberOrdinal);
        addMember(node->resultErrorExtractorMemberOrdinal);
        addMember(node->resultErrorFactoryMemberOrdinal);
        addMember(node->resultErrorConversionMemberOrdinal);
        if (includeDisposeContract) addMember(node->disposeContractMemberOrdinal);
        addMember(node->disposeImplementationMemberOrdinal);
        addMember(node->allocatorDeallocateMemberOrdinal);
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            if (child->get() != nullptr) nodes.push_back(child->get());
    }
}
}

std::vector<std::uint8_t> projectOopReachableDefinitionMask(
    const frontend::ProjectOopMirModel& mirModel)
{
    const std::span<const frontend::ProjectOopMirFunction> functions = mirModel.functions();
    std::vector<std::uint8_t> reachable(functions.size(), 0U);
    std::unordered_map<std::string_view, std::vector<std::size_t>> functionsByName;
    std::unordered_map<std::size_t, std::size_t> functionByMember;
    functionsByName.reserve(functions.size());
    functionByMember.reserve(functions.size());
    for (std::size_t ordinal = 0; ordinal < functions.size(); ++ordinal)
    {
        const frontend::ProjectOopMirFunction& function = functions[ordinal];
        functionsByName[function.mangledName].push_back(ordinal);
        if (!function.isLambda && !function.namespaceFunctionOrdinal.has_value())
            functionByMember.emplace(function.memberOrdinal, ordinal);
    }

    std::vector<std::size_t> work;
    work.reserve(functions.size());
    const auto addFunction = [&](std::size_t ordinal)
    {
        if (ordinal >= functions.size()) return;
        const frontend::ProjectOopMirFunction& function = functions[ordinal];
        if (function.isImportedC || function.isImportedNative || reachable[ordinal] != 0U) return;
        reachable[ordinal] = 1U;
        work.push_back(ordinal);
    };
    for (std::size_t ordinal = 0; ordinal < functions.size(); ++ordinal)
    {
        const frontend::ProjectOopMirFunction& function = functions[ordinal];
        if (!function.isTransportedInlineBody) addFunction(ordinal);
    }

    while (!work.empty())
    {
        const std::size_t functionOrdinal = work.back();
        work.pop_back();
        const frontend::ProjectOopMirFunction& function = functions[functionOrdinal];
        const auto addCallee = [&](std::string_view name)
        {
            if (name.empty()) return;
            const auto found = functionsByName.find(name);
            if (found == functionsByName.end()) return;
            for (const std::size_t ordinal : found->second) addFunction(ordinal);
        };
        const auto addMember = [&](std::optional<std::size_t> member)
        {
            if (!member.has_value()) return;
            const auto found = functionByMember.find(*member);
            if (found != functionByMember.end()) addFunction(found->second);
        };
        const auto scanNode = [&](const frontend::ProjectOopMirNode* root)
        {
            scanFunctionReferences(root, mirModel, addCallee, addMember, false);
        };
        scanNode(function.body.get());
        for (const frontend::ProjectOopMirCleanupRegion& cleanup : function.cleanupRegions)
            scanNode(cleanup.action);
    }
    return reachable;
}

std::vector<std::uint8_t> projectOopStaticFieldDeclarationMask(
    const frontend::ProjectOopMirModel& mirModel,
    std::span<const std::size_t> definitionOrdinals,
    bool partitioned,
    bool ownsModuleGlobals)
{
    const std::span<const frontend::ProjectOopMirStaticField> fields = mirModel.staticFields();
    std::vector<std::uint8_t> selected(fields.size(), 0U);
    if (!partitioned)
    {
        std::fill(selected.begin(), selected.end(), 1U);
        return selected;
    }

    std::unordered_map<std::size_t, std::size_t> fieldByMember;
    fieldByMember.reserve(fields.size());
    for (std::size_t ordinal = 0; ordinal < fields.size(); ++ordinal)
        fieldByMember.emplace(fields[ordinal].memberOrdinal, ordinal);

    const auto addMember = [&](std::optional<std::size_t> member)
    {
        if (!member.has_value()) return;
        const auto found = fieldByMember.find(*member);
        if (found != fieldByMember.end()) selected[found->second] = 1U;
    };
    const auto scanNode = [&](const frontend::ProjectOopMirNode* root)
    {
        std::vector<const frontend::ProjectOopMirNode*> work;
        if (root != nullptr) work.push_back(root);
        while (!work.empty())
        {
            const frontend::ProjectOopMirNode* node = work.back();
            work.pop_back();
            if (node == nullptr) continue;
            addMember(node->memberOrdinal);
            addMember(node->secondaryMemberOrdinal);
            addMember(node->resultSuccessTestMemberOrdinal);
            addMember(node->resultSuccessExtractorMemberOrdinal);
            addMember(node->resultErrorExtractorMemberOrdinal);
            addMember(node->resultErrorFactoryMemberOrdinal);
            addMember(node->resultErrorConversionMemberOrdinal);
            addMember(node->disposeContractMemberOrdinal);
            addMember(node->disposeImplementationMemberOrdinal);
            addMember(node->allocatorDeallocateMemberOrdinal);
            for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
                if (child->get() != nullptr) work.push_back(child->get());
        }
    };

    if (ownsModuleGlobals)
    {
        for (std::size_t ordinal = 0; ordinal < fields.size(); ++ordinal)
            if (!fields[ordinal].isImported || fields[ordinal].isConst) selected[ordinal] = 1U;
        for (const frontend::ProjectOopMirFieldInitializer& initializer : mirModel.fieldInitializers())
        {
            const auto target = fieldByMember.find(initializer.memberOrdinal);
            if (target == fieldByMember.end() || fields[target->second].isImported) continue;
            selected[target->second] = 1U;
            scanNode(initializer.value.get());
        }
        return selected;
    }

    const std::span<const frontend::ProjectOopMirFunction> functions = mirModel.functions();
    for (const std::size_t ordinal : definitionOrdinals)
    {
        if (ordinal >= functions.size()) continue;
        const frontend::ProjectOopMirFunction& function = functions[ordinal];
        scanNode(function.body.get());
        for (const frontend::ProjectOopMirCleanupRegion& cleanup : function.cleanupRegions)
            scanNode(cleanup.action);
        for (const frontend::ProjectOopMirNonlocalExit& exit : function.exceptionFlow.nonlocalExits)
            scanNode(exit.node);
    }
    return selected;
}

std::vector<std::size_t> projectOopCoordinatorFunctionDeclarations(
    const frontend::ProjectOopMirModel& mirModel)
{
    const std::span<const frontend::ProjectOopMirFunction> functions = mirModel.functions();
    const std::vector<std::uint8_t> reachableDefinitions =
        projectOopReachableDefinitionMask(mirModel);
    std::vector<std::uint8_t> selected(functions.size(), 0U);
    std::unordered_map<std::string_view, std::vector<std::size_t>> functionsByName;
    std::unordered_map<std::size_t, std::size_t> functionByMember;
    functionsByName.reserve(functions.size());
    functionByMember.reserve(functions.size());
    for (std::size_t ordinal = 0; ordinal < functions.size(); ++ordinal)
    {
        const frontend::ProjectOopMirFunction& function = functions[ordinal];
        functionsByName[function.mangledName].push_back(ordinal);
        if (!function.isLambda && !function.namespaceFunctionOrdinal.has_value())
            functionByMember.emplace(function.memberOrdinal, ordinal);
        if (!function.isImportedC && !function.isImportedNative &&
            ordinal < reachableDefinitions.size() && reachableDefinitions[ordinal] != 0U)
            selected[ordinal] = 1U;
    }

    const auto addName = [&](std::string_view name)
    {
        if (name.empty()) return;
        const auto found = functionsByName.find(name);
        if (found == functionsByName.end()) return;
        for (const std::size_t ordinal : found->second) selected[ordinal] = 1U;
    };
    const auto addMember = [&](std::optional<std::size_t> member)
    {
        if (!member.has_value()) return;
        const auto found = functionByMember.find(*member);
        if (found != functionByMember.end()) selected[found->second] = 1U;
    };
    const auto scanNode = [&](const frontend::ProjectOopMirNode* root)
    {
        scanFunctionReferences(root, mirModel, addName, addMember, true);
    };
    const std::span<const frontend::ProjectOopMirStaticField> fields = mirModel.staticFields();
    std::unordered_map<std::size_t, const frontend::ProjectOopMirStaticField*> fieldByMember;
    fieldByMember.reserve(fields.size());
    for (const frontend::ProjectOopMirStaticField& field : fields)
        fieldByMember.emplace(field.memberOrdinal, &field);
    for (const frontend::ProjectOopMirFieldInitializer& initializer : mirModel.fieldInitializers())
    {
        const auto field = fieldByMember.find(initializer.memberOrdinal);
        if (field == fieldByMember.end() || field->second == nullptr || field->second->isImported)
            continue;
        scanNode(initializer.value.get());
    }
    for (const frontend::ProjectOopMirDispatchTable& table : mirModel.dispatchTables())
    {
        if (table.isImported) continue;
        for (const std::string& callee : table.slotCalleeNames) addName(callee);
        for (const std::string& callee : table.destructorCalleeNames) addName(callee);
        for (const frontend::ProjectOopMirDispatchTable::InterfaceTable& interfaceTable :
             table.interfaceTables)
            for (const std::string& callee : interfaceTable.calleeNames) addName(callee);
    }

    std::vector<std::size_t> ordinals;
    ordinals.reserve(functions.size());
    for (std::size_t ordinal = 0; ordinal < selected.size(); ++ordinal)
        if (selected[ordinal] != 0U) ordinals.push_back(ordinal);
    return ordinals;
}
}
