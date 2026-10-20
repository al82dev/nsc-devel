#include "nativesharp/LLVM/ProjectOopCodePartition.h"

#include "Internal/PartitionCandidate.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/LLVM/ProjectOopFunctionReachability.h"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace nativesharp::llvm_backend
{
namespace
{
std::uint64_t saturatedAdd(std::uint64_t left, std::uint64_t right) noexcept
{
    const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
    return right > maximum - left ? maximum : left + right;
}

std::uint64_t nodeCost(const frontend::ProjectOopMirNode* node) noexcept
{
    if (node == nullptr) return 0;
    // NSC305-L02. Iterative walk (no recursion in the partition hot path).
    std::uint64_t cost = 0;
    std::vector<const frontend::ProjectOopMirNode*> work;
    work.push_back(node);
    while (!work.empty())
    {
        const frontend::ProjectOopMirNode* current = work.back();
        work.pop_back();
        if (current == nullptr) continue;
        cost = saturatedAdd(cost, 1);
        for (auto it = current->children.rbegin(); it != current->children.rend(); ++it)
            if (it->get() != nullptr) work.push_back(it->get());
    }
    return cost;
}

std::string codeUnitKey(std::size_t ordinal)
{
    std::ostringstream stream;
    stream << "code-" << std::setw(4) << std::setfill('0') << ordinal;
    return stream.str();
}

std::uint64_t stableNameHash(std::string_view name) noexcept
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char character : name)
    {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(character));
        hash *= 1099511628211ULL;
    }
    return hash;
}

// NSC305-L01. Bounded historical weight: a 1:1 blend of the previous historical cost and the
// observed cost, saturated at `cap`, so a hot unit's weight cannot grow unbounded across builds.
std::uint64_t boundedHistoricalWeight(
    std::uint64_t previous, std::uint64_t observed, std::uint64_t cap) noexcept
{
    const std::uint64_t blended =
        previous / 2U + observed / 2U + ((previous & 1U) + (observed & 1U)) / 2U;
    return blended > cap ? cap : blended;
}

std::vector<std::size_t> stableCandidateOrder(
    std::span<const PartitionCandidate> candidates)
{
    std::vector<std::size_t> order(candidates.size());
    std::iota(order.begin(), order.end(), 0U);
    std::sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right) {
        if (candidates[left].stableName != candidates[right].stableName)
            return candidates[left].stableName < candidates[right].stableName;
        return candidates[left].functionOrdinal < candidates[right].functionOrdinal;
    });
    return order;
}

bool parsePersistentPlan(
    std::string_view payload,
    std::size_t expectedUnitCount,
    std::size_t expectedFunctionCount,
    std::vector<std::uint64_t>& historicalCosts,
    std::vector<std::size_t>& assignments)
{
    std::istringstream stream{std::string(payload)};
    std::string magic;
    std::string unitsLabel;
    std::string functionsLabel;
    std::string historyLabel;
    std::string assignmentsLabel;
    std::size_t version = 0;
    std::size_t unitCount = 0;
    std::size_t functionCount = 0;
    if (!(stream >> magic >> version >> unitsLabel >> unitCount >>
            functionsLabel >> functionCount >> historyLabel) ||
        magic != "NSCPARTITION" || version != 1U || unitsLabel != "units" ||
        functionsLabel != "functions" || historyLabel != "history" ||
        unitCount != expectedUnitCount || functionCount != expectedFunctionCount)
        return false;
    historicalCosts.resize(unitCount);
    for (std::uint64_t& cost : historicalCosts)
        if (!(stream >> cost)) return false;
    if (!(stream >> assignmentsLabel) || assignmentsLabel != "assignments") return false;
    assignments.resize(functionCount);
    for (std::size_t& assignment : assignments)
        if (!(stream >> assignment) || assignment >= unitCount) return false;
    std::string trailing;
    return !(stream >> trailing);
}

void collectPartitionCandidates(
    const frontend::ProjectOopMirModel& mirModel,
    std::span<const std::uint8_t> reachableDefinitions,
    std::vector<PartitionCandidate>& candidates)
{
    const std::span<const frontend::ProjectOopMirFunction> functions = mirModel.functions();
    candidates.reserve(functions.size());
    for (std::size_t ordinal = 0; ordinal < functions.size(); ++ordinal)
    {
        const frontend::ProjectOopMirFunction& function = functions[ordinal];
        if (function.isImportedC || function.isImportedNative ||
            ordinal >= reachableDefinitions.size() || reachableDefinitions[ordinal] == 0U) continue;
        candidates.push_back(PartitionCandidate{
            ordinal, std::max<std::uint64_t>(1, nodeCost(function.body.get())),
            function.mangledName});
    }
}

void assignBalancedCandidates(
    std::vector<PartitionCandidate>& candidates,
    std::vector<ProjectOopCodeUnit>& units)
{
    std::sort(candidates.begin(), candidates.end(), [](const PartitionCandidate& left,
                                                       const PartitionCandidate& right) {
        if (left.cost != right.cost) return left.cost > right.cost;
        if (left.stableName != right.stableName) return left.stableName < right.stableName;
        return left.functionOrdinal < right.functionOrdinal;
    });
    for (const PartitionCandidate& candidate : candidates)
    {
        const auto selected = std::min_element(
            units.begin(), units.end(), [](const ProjectOopCodeUnit& left,
                                          const ProjectOopCodeUnit& right) {
                if (left.estimatedCost != right.estimatedCost)
                    return left.estimatedCost < right.estimatedCost;
                return left.ordinal < right.ordinal;
            });
        selected->estimatedCost = saturatedAdd(selected->estimatedCost, candidate.cost);
        selected->functionOrdinals.push_back(candidate.functionOrdinal);
    }
}

void assignStableCandidates(
    std::vector<PartitionCandidate>& candidates,
    std::vector<ProjectOopCodeUnit>& units)
{
    // Identity hashing keeps a body-only edit in the same bucket. Empty buckets are omitted
    // without renumbering surviving keys, so unrelated object-cache identities stay stable.
    std::sort(candidates.begin(), candidates.end(), [](const PartitionCandidate& left,
                                                       const PartitionCandidate& right) {
        if (left.stableName != right.stableName) return left.stableName < right.stableName;
        return left.functionOrdinal < right.functionOrdinal;
    });
    for (const PartitionCandidate& candidate : candidates)
    {
        const std::size_t bucket = static_cast<std::size_t>(
            stableNameHash(candidate.stableName) % units.size());
        units[bucket].estimatedCost = saturatedAdd(units[bucket].estimatedCost, candidate.cost);
        units[bucket].functionOrdinals.push_back(candidate.functionOrdinal);
    }
    std::erase_if(units, [](const ProjectOopCodeUnit& unit) {
        return unit.functionOrdinals.empty();
    });
}

bool restoreCacheAwareCandidates(
    std::span<const PartitionCandidate> candidates,
    std::string_view cachedPlan,
    std::vector<ProjectOopCodeUnit>& units,
    std::vector<std::size_t>& persistentFunctionOrdinals)
{
    const std::vector<std::size_t> stableOrder = stableCandidateOrder(candidates);
    persistentFunctionOrdinals.reserve(stableOrder.size());
    for (const std::size_t index : stableOrder)
        persistentFunctionOrdinals.push_back(candidates[index].functionOrdinal);
    std::vector<std::uint64_t> historicalCosts;
    std::vector<std::size_t> assignments;
    if (!parsePersistentPlan(
            cachedPlan, units.size(), candidates.size(), historicalCosts, assignments)) return false;
    std::vector<std::size_t> functionsPerUnit(units.size(), 0U);
    for (std::size_t index = 0; index < stableOrder.size(); ++index)
    {
        const PartitionCandidate& candidate = candidates[stableOrder[index]];
        ProjectOopCodeUnit& unit = units[assignments[index]];
        unit.estimatedCost = saturatedAdd(unit.estimatedCost, candidate.cost);
        unit.functionOrdinals.push_back(candidate.functionOrdinal);
        ++functionsPerUnit[assignments[index]];
    }
    if (std::ranges::any_of(functionsPerUnit, [](std::size_t count) { return count == 0U; }))
    {
        for (ProjectOopCodeUnit& unit : units)
        {
            unit.estimatedCost = 0;
            unit.functionOrdinals.clear();
        }
        return false;
    }
    for (std::size_t ordinal = 0; ordinal < units.size(); ++ordinal)
        units[ordinal].historicalCost = historicalCosts[ordinal];
    return true;
}

void finalizePartitionUnits(
    std::vector<ProjectOopCodeUnit>& units,
    std::vector<std::optional<std::size_t>>& unitByFunction,
    bool restoredCacheAwarePlan)
{
    // NSC305-L01. Blend the observed cost into the bounded historical weight (a capped 1:1 moving
    // average). A first observation seeds it directly.
    constexpr std::uint64_t kHistoricalCostCap = std::uint64_t{1} << 24U;
    for (ProjectOopCodeUnit& unit : units)
    {
        if (!restoredCacheAwarePlan)
        {
            unit.historicalCost = unit.historicalCost == 0U
                ? std::min(unit.estimatedCost, kHistoricalCostCap)
                : boundedHistoricalWeight(
                    unit.historicalCost, unit.estimatedCost, kHistoricalCostCap);
        }
        std::sort(unit.functionOrdinals.begin(), unit.functionOrdinals.end());
        for (const std::size_t functionOrdinal : unit.functionOrdinals)
            unitByFunction[functionOrdinal] = unit.ordinal;
    }
}

std::string_view fixedArrayElementType(std::string_view typeKey)
{
    if (typeKey.size() < 3 || typeKey.back() != ']') return {};
    const std::size_t open = typeKey.rfind('[');
    if (open == std::string_view::npos || open == 0 || open + 1 >= typeKey.size() - 1) return {};
    for (std::size_t index = open + 1; index + 1 < typeKey.size(); ++index)
        if (typeKey[index] < '0' || typeKey[index] > '9') return {};
    return typeKey.substr(0, open);
}

void addPartitionMemberDependency(
    std::optional<std::size_t> member,
    const std::unordered_map<std::size_t, std::size_t>& functionByMember,
    std::unordered_set<std::size_t>& dependencies)
{
    if (!member.has_value()) return;
    const auto found = functionByMember.find(*member);
    if (found != functionByMember.end()) dependencies.insert(found->second);
}

void collectPartitionNodeDependencies(
    const frontend::ProjectOopMirNode* root,
    const frontend::ProjectOopMirModel& mirModel,
    const std::unordered_map<std::string_view, std::vector<std::size_t>>& functionsByName,
    const std::unordered_map<std::size_t, std::size_t>& functionByMember,
    const std::unordered_map<std::size_t, std::size_t>& destructorFunctionByType,
    const std::unordered_map<std::string_view, const frontend::ProjectOopMirStructLayout*>& layouts,
    std::unordered_set<std::size_t>& dependencies)
{
    // NSC305-L02. Iterative walk (no recursion in the partition hot path).
    std::vector<const frontend::ProjectOopMirNode*> work;
    if (root != nullptr) work.push_back(root);
    while (!work.empty())
    {
        const frontend::ProjectOopMirNode* node = work.back();
        work.pop_back();
        if (node == nullptr) continue;
        const auto addCallee = [&](std::string_view name)
        {
            if (name.empty()) return;
            const auto found = functionsByName.find(name);
            if (found != functionsByName.end())
                dependencies.insert(found->second.begin(), found->second.end());
        };
        addCallee(node->calleeName);
        addCallee(node->secondaryCalleeName);
        addCallee(node->resultSuccessTestCalleeName);
        addCallee(node->resultSuccessExtractorCalleeName);
        addCallee(node->resultErrorExtractorCalleeName);
        addCallee(node->resultErrorFactoryCalleeName);
        addCallee(node->resultErrorConversionCalleeName);
        // NS304-T05-F09: a PollAddress references the async poll function by name (stringValue),
        // so the unit that lowers it must also declare that poll for the cross-unit pointer store.
        if (node->kind == frontend::ProjectOopMirNodeKind::PollAddress) addCallee(node->stringValue);
        if (node->kind == frontend::ProjectOopMirNodeKind::Panic)
            addCallee(mirModel.panicHandlerMangledName());
        addPartitionMemberDependency(node->memberOrdinal, functionByMember, dependencies);
        addPartitionMemberDependency(node->secondaryMemberOrdinal, functionByMember, dependencies);
        addPartitionMemberDependency(node->resultSuccessTestMemberOrdinal, functionByMember, dependencies);
        addPartitionMemberDependency(node->resultSuccessExtractorMemberOrdinal, functionByMember, dependencies);
        addPartitionMemberDependency(node->resultErrorExtractorMemberOrdinal, functionByMember, dependencies);
        addPartitionMemberDependency(node->resultErrorFactoryMemberOrdinal, functionByMember, dependencies);
        addPartitionMemberDependency(node->resultErrorConversionMemberOrdinal, functionByMember, dependencies);
        addPartitionMemberDependency(node->disposeImplementationMemberOrdinal, functionByMember, dependencies);
        // NS304-T13-F07: the placement construction references the allocator's Deallocate member
        // for its generated thunk, so that member must be declared in the same code unit.
        addPartitionMemberDependency(node->allocatorDeallocateMemberOrdinal, functionByMember, dependencies);
        if (node->automaticDestruction)
        {
            std::unordered_set<std::size_t> seen;
            std::vector<std::string_view> typeWork{node->typeKey};
            while (!typeWork.empty())
            {
                const std::string_view typeKey = typeWork.back();
                typeWork.pop_back();
                if (const std::string_view element = fixedArrayElementType(typeKey); !element.empty())
                {
                    typeWork.push_back(element);
                    continue;
                }
                const auto layoutFound = layouts.find(typeKey);
                if (layoutFound == layouts.end()) continue;
                const frontend::ProjectOopMirStructLayout& layout = *layoutFound->second;
                if (!seen.insert(layout.typeOrdinal).second) continue;
                if (const auto destructor = destructorFunctionByType.find(layout.typeOrdinal);
                    destructor != destructorFunctionByType.end()) dependencies.insert(destructor->second);
                for (const frontend::ObjectFieldLayout& field : layout.fields)
                    typeWork.push_back(field.typeKey);
            }
        }
        for (auto it = node->children.rbegin(); it != node->children.rend(); ++it)
            if (it->get() != nullptr) work.push_back(it->get());
    }
}

void populateUnitDeclarations(
    const frontend::ProjectOopMirModel& mirModel,
    std::vector<ProjectOopCodeUnit>& units)
{
    const std::span<const frontend::ProjectOopMirFunction> functions = mirModel.functions();
    std::unordered_map<std::string_view, std::vector<std::size_t>> functionsByName;
    std::unordered_map<std::size_t, std::size_t> functionByMember;
    std::unordered_map<std::size_t, std::size_t> destructorFunctionByType;
    std::unordered_map<std::string_view, const frontend::ProjectOopMirStructLayout*> layouts;
    for (std::size_t ordinal = 0; ordinal < functions.size(); ++ordinal)
    {
        functionsByName[functions[ordinal].mangledName].push_back(ordinal);
        if (!functions[ordinal].isLambda && !functions[ordinal].namespaceFunctionOrdinal.has_value())
            functionByMember.emplace(functions[ordinal].memberOrdinal, ordinal);
        if (functions[ordinal].destructor)
            destructorFunctionByType.emplace(functions[ordinal].declaringTypeOrdinal, ordinal);
    }
    for (const frontend::ProjectOopMirStructLayout& layout : mirModel.structLayouts())
        layouts.emplace(layout.typeKey, &layout);
    for (ProjectOopCodeUnit& unit : units)
    {
        std::unordered_set<std::size_t> dependencies(
            unit.functionOrdinals.begin(), unit.functionOrdinals.end());
        for (const std::size_t functionOrdinal : unit.functionOrdinals)
        {
            if (functionOrdinal >= functions.size()) continue;
            const frontend::ProjectOopMirFunction& function = functions[functionOrdinal];
            collectPartitionNodeDependencies(
                function.body.get(), mirModel, functionsByName, functionByMember,
                destructorFunctionByType, layouts, dependencies);
            for (const frontend::ProjectOopMirCleanupRegion& cleanup : function.cleanupRegions)
                collectPartitionNodeDependencies(
                    cleanup.action, mirModel, functionsByName, functionByMember,
                    destructorFunctionByType, layouts, dependencies);
        }
        unit.declarationOrdinals.assign(dependencies.begin(), dependencies.end());
        std::sort(unit.declarationOrdinals.begin(), unit.declarationOrdinals.end());
    }
}
}

bool ProjectOopCodePartition::build(
    const frontend::ProjectOopMirModel& mirModel,
    std::size_t requestedUnitCount,
    std::string& error,
    ProjectOopCodePartitionPolicy policy,
    std::string_view cachedPlan,
    std::span<const std::uint8_t> sharedReachableDefinitions)
{
    error.clear();
    if (built_)
    {
        error = "ProjectOopCodePartition instances are single-use";
        return false;
    }
    if (!mirModel.isBuilt())
    {
        error = "module code partitioning requires a built OOP MIR model";
        return false;
    }

    const std::span<const frontend::ProjectOopMirFunction> functions = mirModel.functions();
    std::vector<std::uint8_t> ownedReachableDefinitions;
    if (sharedReachableDefinitions.empty() && !functions.empty())
    {
        ownedReachableDefinitions = projectOopReachableDefinitionMask(mirModel);
        sharedReachableDefinitions = ownedReachableDefinitions;
    }
    unitByFunction_.resize(functions.size());
    std::vector<PartitionCandidate> candidates;
    collectPartitionCandidates(mirModel, sharedReachableDefinitions, candidates);

    if (candidates.empty())
    {
        built_ = true;
        return true;
    }

    const std::size_t unitCount = std::min(
        std::max<std::size_t>(1, requestedUnitCount), candidates.size());
    units_.reserve(unitCount);
    for (std::size_t ordinal = 0; ordinal < unitCount; ++ordinal)
        units_.push_back(ProjectOopCodeUnit{ordinal, codeUnitKey(ordinal), 0, 0, {}, {}});

    bool restoredCacheAwarePlan = false;

    if (policy == ProjectOopCodePartitionPolicy::StableIncremental)
    {
        assignStableCandidates(candidates, units_);
    }
    else if (policy == ProjectOopCodePartitionPolicy::CacheAwareBalanced)
    {
        restoredCacheAwarePlan = restoreCacheAwareCandidates(
            candidates, cachedPlan, units_, persistentFunctionOrdinals_);
        if (!restoredCacheAwarePlan) assignBalancedCandidates(candidates, units_);
        persistentUnitCount_ = unitCount;
        persistentPlanReady_ = true;
    }
    else
    {
        // Longest-processing-time partitioning gives useful balance while remaining fully stable:
        // cost, physical name and original ordinal are all canonical MIR properties.
        assignBalancedCandidates(candidates, units_);
    }

    finalizePartitionUnits(units_, unitByFunction_, restoredCacheAwarePlan);

    populateUnitDeclarations(mirModel, units_);
    built_ = true;
    return true;
}

std::string ProjectOopCodePartition::persistentPlanKey(
    const frontend::ProjectOopMirModel& mirModel,
    std::size_t requestedUnitCount,
    std::span<const std::uint8_t> sharedReachableDefinitions)
{
    std::vector<std::pair<std::string_view, std::size_t>> identities;
    const std::span<const frontend::ProjectOopMirFunction> functions = mirModel.functions();
    std::vector<std::uint8_t> ownedReachableDefinitions;
    if (sharedReachableDefinitions.empty() && !functions.empty())
    {
        ownedReachableDefinitions = projectOopReachableDefinitionMask(mirModel);
        sharedReachableDefinitions = ownedReachableDefinitions;
    }
    identities.reserve(functions.size());
    for (std::size_t ordinal = 0; ordinal < functions.size(); ++ordinal)
    {
        if (functions[ordinal].isImportedC || functions[ordinal].isImportedNative ||
            ordinal >= sharedReachableDefinitions.size() ||
            sharedReachableDefinitions[ordinal] == 0U)
            continue;
        identities.emplace_back(functions[ordinal].mangledName, ordinal);
    }
    std::sort(identities.begin(), identities.end(), [](const auto& left, const auto& right) {
        if (left.first != right.first) return left.first < right.first;
        return left.second < right.second;
    });
    const std::size_t unitCount = identities.empty() ? 0U : std::min(
        std::max<std::size_t>(1, requestedUnitCount), identities.size());
    std::string material = "NSCPARTITIONKEY 1\nunits " + std::to_string(unitCount) +
        "\nfunctions " + std::to_string(identities.size()) + "\n";
    for (const auto& [name, ordinal] : identities)
    {
        material += std::to_string(name.size()) + ":";
        material.append(name);
        material += ":" + std::to_string(ordinal) + "\n";
    }
    const auto bytes = std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(material.data()), material.size());
    return basic::sha256Hex(basic::sha256(bytes));
}

std::span<const ProjectOopCodeUnit> ProjectOopCodePartition::units() const noexcept
{
    return units_;
}

std::optional<std::size_t> ProjectOopCodePartition::unitForFunction(
    std::size_t functionOrdinal) const noexcept
{
    return functionOrdinal < unitByFunction_.size() ? unitByFunction_[functionOrdinal]
                                                     : std::nullopt;
}

void ProjectOopCodePartition::recordCodeUnitCost(
    std::size_t ordinal, std::uint64_t observedCost) noexcept
{
    const auto found = std::find_if(units_.begin(), units_.end(), [&](const auto& unit) {
        return unit.ordinal == ordinal;
    });
    if (found == units_.end()) return;
    constexpr std::uint64_t kHistoricalCostCap = std::uint64_t{1} << 24U;
    ProjectOopCodeUnit& unit = *found;
    unit.historicalCost = unit.historicalCost == 0U
        ? std::min(observedCost, kHistoricalCostCap)
        : boundedHistoricalWeight(unit.historicalCost, observedCost, kHistoricalCostCap);
}

std::string ProjectOopCodePartition::persistentPlanPayload() const
{
    if (!built_ || !persistentPlanReady_ || persistentUnitCount_ == 0U) return {};
    std::vector<std::uint64_t> historicalCosts(persistentUnitCount_, 0U);
    for (const ProjectOopCodeUnit& unit : units_)
    {
        if (unit.ordinal >= historicalCosts.size()) return {};
        historicalCosts[unit.ordinal] = unit.historicalCost;
    }
    std::ostringstream stream;
    stream << "NSCPARTITION 1\nunits " << persistentUnitCount_ << "\nfunctions " <<
        persistentFunctionOrdinals_.size() << "\nhistory";
    for (const std::uint64_t cost : historicalCosts) stream << ' ' << cost;
    stream << "\nassignments";
    for (const std::size_t functionOrdinal : persistentFunctionOrdinals_)
    {
        if (functionOrdinal >= unitByFunction_.size() ||
            !unitByFunction_[functionOrdinal].has_value())
            return {};
        stream << ' ' << *unitByFunction_[functionOrdinal];
    }
    stream << '\n';
    return stream.str();
}

std::string ProjectOopCodePartition::dump() const
{
    std::ostringstream stream;
    stream << "project-oop-code-partition units=" << units_.size() << '\n';
    for (const ProjectOopCodeUnit& unit : units_)
    {
        stream << "  " << unit.stableKey << " cost=" << unit.estimatedCost << " functions=";
        for (std::size_t index = 0; index < unit.functionOrdinals.size(); ++index)
        {
            if (index != 0) stream << ',';
            stream << unit.functionOrdinals[index];
        }
        stream << " declarations=" << unit.declarationOrdinals.size() << '\n';
    }
    return stream.str();
}

bool ProjectOopCodePartition::isBuilt() const noexcept
{
    return built_;
}
}
