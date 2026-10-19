#include "nativesharp/Frontend/ProjectCostExplorer.h"

#include <algorithm>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
bool isAtomic(ProjectOopMirNodeKind kind)
{
    switch (kind)
    {
        case ProjectOopMirNodeKind::CoreAtomicLoadInt32:
        case ProjectOopMirNodeKind::CoreAtomicStoreInt32:
        case ProjectOopMirNodeKind::CoreInterlockedIncrementInt32:
        case ProjectOopMirNodeKind::CoreInterlockedDecrementInt32:
        case ProjectOopMirNodeKind::CoreInterlockedAddInt32:
        case ProjectOopMirNodeKind::CoreInterlockedExchangeInt32:
        case ProjectOopMirNodeKind::CoreInterlockedCompareExchangeInt32:
        case ProjectOopMirNodeKind::CoreAtomicLoadInt64:
        case ProjectOopMirNodeKind::CoreAtomicStoreInt64:
        case ProjectOopMirNodeKind::CoreInterlockedIncrementInt64:
        case ProjectOopMirNodeKind::CoreInterlockedDecrementInt64:
        case ProjectOopMirNodeKind::CoreInterlockedAddInt64:
        case ProjectOopMirNodeKind::CoreInterlockedExchangeInt64:
        case ProjectOopMirNodeKind::CoreInterlockedCompareExchangeInt64:
        case ProjectOopMirNodeKind::CoreAtomicLoadPointer:
        case ProjectOopMirNodeKind::CoreAtomicStorePointer:
        case ProjectOopMirNodeKind::CoreInterlockedExchangePointer:
        case ProjectOopMirNodeKind::CoreInterlockedCompareExchangePointer:
        case ProjectOopMirNodeKind::CoreAtomicThreadFence:
        case ProjectOopMirNodeKind::CoreAtomicCompilerFence:
            return true;
        default:
            return false;
    }
}

bool isLock(ProjectOopMirNodeKind kind)
{
    switch (kind)
    {
        case ProjectOopMirNodeKind::CoreMutexCreate:
        case ProjectOopMirNodeKind::CoreMutexCreateNamed:
        case ProjectOopMirNodeKind::CoreMutexLock:
        case ProjectOopMirNodeKind::CoreMutexUnlock:
        case ProjectOopMirNodeKind::CoreMutexClose:
        case ProjectOopMirNodeKind::CoreMonitorEnter:
        case ProjectOopMirNodeKind::CoreMonitorExit:
        case ProjectOopMirNodeKind::CoreMonitorWait:
        case ProjectOopMirNodeKind::CoreMonitorPulse:
        case ProjectOopMirNodeKind::CoreMonitorPulseAll:
        case ProjectOopMirNodeKind::CoreSemaphoreCreate:
        case ProjectOopMirNodeKind::CoreSemaphoreCreateNamed:
        case ProjectOopMirNodeKind::CoreSemaphoreWait:
        case ProjectOopMirNodeKind::CoreSemaphoreRelease:
        case ProjectOopMirNodeKind::CoreSemaphoreClose:
        case ProjectOopMirNodeKind::CoreEventCreate:
        case ProjectOopMirNodeKind::CoreEventCreateNamed:
        case ProjectOopMirNodeKind::CoreEventSet:
        case ProjectOopMirNodeKind::CoreEventReset:
        case ProjectOopMirNodeKind::CoreEventWait:
        case ProjectOopMirNodeKind::CoreEventClose:
            return true;
        default:
            return false;
    }
}

void addCause(ProjectFunctionCosts& costs, std::string_view category, ProjectOopMirNodeKind kind)
{
    costs.causes.push_back(std::string(category) + " <- " + projectOopMirNodeKindName(kind));
}

void accumulateSynchronizationCost(const ProjectOopMirNode& node, ProjectFunctionCosts& costs)
{
    if (isAtomic(node.kind))
    {
        ++costs.atomicSites;
        addCause(costs, "atomic", node.kind);
    }
    if (isLock(node.kind))
    {
        ++costs.lockSites;
        addCause(costs, "lock", node.kind);
    }
}

void accumulateNodeCost(const ProjectOopMirNode& node, ProjectFunctionCosts& costs)
{
    switch (node.kind)
    {
        case ProjectOopMirNodeKind::StackAllocation:
            costs.staticStackBytesLowerBound += node.variableLengthArray ? 0 : node.immediate;
            addCause(costs, "stack", node.kind);
            break;
        case ProjectOopMirNodeKind::PlacementConstruct:
            ++costs.arenaAllocationSites;
            addCause(costs, "arena", node.kind);
            break;
        case ProjectOopMirNodeKind::RawAllocation:
        case ProjectOopMirNodeKind::ArrayAllocate:
        case ProjectOopMirNodeKind::RangeArrayAllocate:
        case ProjectOopMirNodeKind::StringConcat:
        case ProjectOopMirNodeKind::StringSubstring:
        case ProjectOopMirNodeKind::StringFromCharArray:
            ++costs.heapAllocationSites;
            addCause(costs, "heap", node.kind);
            break;
        case ProjectOopMirNodeKind::AllocateAndConstruct:
            if (node.stackPromotedObject)
            {
                costs.staticStackBytesLowerBound += node.immediate;
                addCause(costs, "stack", node.kind);
            }
            else
            {
                ++costs.heapAllocationSites;
                addCause(costs, "heap", node.kind);
            }
            break;
        case ProjectOopMirNodeKind::AggregateSpreadStore:
        case ProjectOopMirNodeKind::TupleDeconstructElementStore:
            ++costs.copySites;
            addCause(costs, "copy", node.kind);
            break;
        case ProjectOopMirNodeKind::MoveValue:
        case ProjectOopMirNodeKind::MoveArrayElement:
        case ProjectOopMirNodeKind::MoveOwnedField:
            ++costs.moveSites;
            addCause(costs, "move", node.kind);
            break;
        case ProjectOopMirNodeKind::ObjectDestroy:
        case ProjectOopMirNodeKind::Deallocate:
            ++costs.dropSites;
            addCause(costs, "drop", node.kind);
            break;
        case ProjectOopMirNodeKind::VirtualInstanceCall:
        case ProjectOopMirNodeKind::InterfaceInstanceCall:
        case ProjectOopMirNodeKind::DelegateInvoke:
            ++costs.dynamicDispatchSites;
            addCause(costs, "dynamic-dispatch", node.kind);
            break;
        case ProjectOopMirNodeKind::Throw:
        case ProjectOopMirNodeKind::Rethrow:
        case ProjectOopMirNodeKind::CleanupContinuation:
            ++costs.unwindSites;
            addCause(costs, "unwind", node.kind);
            break;
        default:
            break;
    }
    accumulateSynchronizationCost(node, costs);
}

void visit(const ProjectOopMirNode& root, ProjectFunctionCosts& costs)
{
    std::vector<const ProjectOopMirNode*> pending{&root};
    while (!pending.empty())
    {
        const ProjectOopMirNode* node = pending.back();
        pending.pop_back();
        accumulateNodeCost(*node, costs);
        for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
            if (*child != nullptr) pending.push_back(child->get());
    }
}

void add(ProjectFunctionCosts& target, const ProjectFunctionCosts& source)
{
    target.staticStackBytesLowerBound += source.staticStackBytesLowerBound;
    target.arenaAllocationSites += source.arenaAllocationSites;
    target.heapAllocationSites += source.heapAllocationSites;
    target.copySites += source.copySites;
    target.moveSites += source.moveSites;
    target.dropSites += source.dropSites;
    target.dynamicDispatchSites += source.dynamicDispatchSites;
    target.lockSites += source.lockSites;
    target.atomicSites += source.atomicSites;
    target.unwindSites += source.unwindSites;
    target.monomorphizations += source.monomorphizations;
}

std::string escapeJson(std::string_view value)
{
    std::string result;
    for (const char character : value)
    {
        if (character == '"' || character == '\\') result.push_back('\\');
        if (character == '\n') result += "\\n";
        else if (character == '\r') result += "\\r";
        else if (character == '\t') result += "\\t";
        else result.push_back(character);
    }
    return result;
}

void appendJsonCosts(std::string& result, const ProjectFunctionCosts& costs)
{
    result += "\"stackBytesLowerBound\":" + std::to_string(costs.staticStackBytesLowerBound);
    result += ",\"arenaAllocationSites\":" + std::to_string(costs.arenaAllocationSites);
    result += ",\"heapAllocationSites\":" + std::to_string(costs.heapAllocationSites);
    result += ",\"copySites\":" + std::to_string(costs.copySites);
    result += ",\"moveSites\":" + std::to_string(costs.moveSites);
    result += ",\"dropSites\":" + std::to_string(costs.dropSites);
    result += ",\"dynamicDispatchSites\":" + std::to_string(costs.dynamicDispatchSites);
    result += ",\"lockSites\":" + std::to_string(costs.lockSites);
    result += ",\"atomicSites\":" + std::to_string(costs.atomicSites);
    result += ",\"unwindSites\":" + std::to_string(costs.unwindSites);
    result += ",\"monomorphizations\":" + std::to_string(costs.monomorphizations);
}
}

ProjectCostReport exploreProjectCosts(std::span<const ProjectOopMirFunction> functions)
{
    ProjectCostReport report;
    report.totals.function = "<project>";
    report.functions.reserve(functions.size());
    for (const ProjectOopMirFunction& function : functions)
    {
        ProjectFunctionCosts costs;
        costs.function = !function.debugDisplayName.empty() ? function.debugDisplayName : function.mangledName;
        if (costs.function.empty()) costs.function = "<anonymous>";
        if (!function.genericSpecializationKey.empty())
        {
            costs.monomorphizations = 1;
            costs.causes.push_back("monomorphization <- " + function.genericSpecializationKey);
        }
        for (const ProjectOopMirCleanupRegion& cleanup : function.cleanupRegions)
        {
            if (!cleanup.automaticDestruction) continue;
            ++costs.dropSites;
            costs.causes.push_back("drop <- cleanup-region:" + std::to_string(cleanup.ordinal));
        }
        if (function.exceptionFlow.requiresPersonality)
        {
            ++costs.unwindSites;
            costs.causes.push_back("unwind <- exception-personality");
        }
        if (function.body != nullptr) visit(*function.body, costs);
        std::ranges::sort(costs.causes);
        add(report.totals, costs);
        report.functions.push_back(std::move(costs));
    }
    std::ranges::sort(report.functions, {}, &ProjectFunctionCosts::function);
    return report;
}

std::string projectCostReportText(const ProjectCostReport& report)
{
    std::string result = "NSCOST 1.0\n";
    result += "semantics=mir-static-lower-bound\n";
    result += "functions=" + std::to_string(report.functions.size()) + "\n";
    for (const ProjectFunctionCosts& costs : report.functions)
    {
        result += "function " + costs.function + "\n";
        result += "  stack-bytes-lower-bound=" + std::to_string(costs.staticStackBytesLowerBound) + "\n";
        result += "  arena-sites=" + std::to_string(costs.arenaAllocationSites) + "\n";
        result += "  heap-sites=" + std::to_string(costs.heapAllocationSites) + "\n";
        result += "  copies=" + std::to_string(costs.copySites) + " moves=" +
            std::to_string(costs.moveSites) + " drops=" + std::to_string(costs.dropSites) + "\n";
        result += "  dynamic-dispatch=" + std::to_string(costs.dynamicDispatchSites) +
            " locks=" + std::to_string(costs.lockSites) +
            " atomics=" + std::to_string(costs.atomicSites) +
            " unwind=" + std::to_string(costs.unwindSites) +
            " monomorphizations=" + std::to_string(costs.monomorphizations) + "\n";
        for (const std::string& cause : costs.causes) result += "  cause: " + cause + "\n";
    }
    result += "totals arena-sites=" + std::to_string(report.totals.arenaAllocationSites) +
        " heap-sites=" + std::to_string(report.totals.heapAllocationSites) +
        " copies=" + std::to_string(report.totals.copySites) +
        " moves=" + std::to_string(report.totals.moveSites) +
        " drops=" + std::to_string(report.totals.dropSites) +
        " dynamic-dispatch=" + std::to_string(report.totals.dynamicDispatchSites) +
        " locks=" + std::to_string(report.totals.lockSites) +
        " atomics=" + std::to_string(report.totals.atomicSites) +
        " unwind=" + std::to_string(report.totals.unwindSites) +
        " monomorphizations=" + std::to_string(report.totals.monomorphizations) + "\n";
    return result;
}

std::string projectCostReportJson(const ProjectCostReport& report)
{
    std::string result = "{\"schema\":\"NSCOST 1.0\",\"semantics\":\"mir-static-lower-bound\",\"functions\":[";
    for (std::size_t index = 0; index < report.functions.size(); ++index)
    {
        if (index != 0) result += ',';
        const ProjectFunctionCosts& costs = report.functions[index];
        result += "{\"function\":\"" + escapeJson(costs.function) + "\",";
        appendJsonCosts(result, costs);
        result += ",\"causes\":[";
        for (std::size_t cause = 0; cause < costs.causes.size(); ++cause)
        {
            if (cause != 0) result += ',';
            result += "\"" + escapeJson(costs.causes[cause]) + "\"";
        }
        result += "]}";
    }
    result += "],\"totals\":{";
    appendJsonCosts(result, report.totals);
    result += "}}\n";
    return result;
}
}
