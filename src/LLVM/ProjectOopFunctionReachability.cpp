#include "nativesharp/LLVM/ProjectOopFunctionReachability.h"

#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include <string_view>
#include <unordered_map>

namespace nativesharp::llvm_backend
{
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
            std::vector<const frontend::ProjectOopMirNode*> nodes;
            if (root != nullptr) nodes.push_back(root);
            while (!nodes.empty())
            {
                const frontend::ProjectOopMirNode* node = nodes.back();
                nodes.pop_back();
                addCallee(node->calleeName);
                addCallee(node->secondaryCalleeName);
                addCallee(node->resultSuccessTestCalleeName);
                addCallee(node->resultSuccessExtractorCalleeName);
                addCallee(node->resultErrorExtractorCalleeName);
                addCallee(node->resultErrorFactoryCalleeName);
                addCallee(node->resultErrorConversionCalleeName);
                if (node->kind == frontend::ProjectOopMirNodeKind::PollAddress)
                    addCallee(node->stringValue);
                if (node->kind == frontend::ProjectOopMirNodeKind::Panic)
                    addCallee(mirModel.panicHandlerMangledName());
                addMember(node->memberOrdinal);
                addMember(node->secondaryMemberOrdinal);
                addMember(node->resultSuccessTestMemberOrdinal);
                addMember(node->resultSuccessExtractorMemberOrdinal);
                addMember(node->resultErrorExtractorMemberOrdinal);
                addMember(node->resultErrorFactoryMemberOrdinal);
                addMember(node->resultErrorConversionMemberOrdinal);
                addMember(node->disposeImplementationMemberOrdinal);
                addMember(node->allocatorDeallocateMemberOrdinal);
                for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
                    nodes.push_back(child->get());
            }
        };
        scanNode(function.body.get());
        for (const frontend::ProjectOopMirCleanupRegion& cleanup : function.cleanupRegions)
            scanNode(cleanup.action);
    }
    return reachable;
}
}
