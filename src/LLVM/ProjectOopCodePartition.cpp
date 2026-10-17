#include "nativesharp/LLVM/ProjectOopCodePartition.h"

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
struct PartitionCandidate final
{
    std::size_t functionOrdinal = 0;
    std::uint64_t cost = 0;
    std::string_view stableName;
};

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
        cost = saturatedAdd(cost, 1);
        for (auto it = current->children.rbegin(); it != current->children.rend(); ++it)
            work.push_back(it->get());
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
}

bool ProjectOopCodePartition::build(
    const frontend::ProjectOopMirModel& mirModel,
    std::size_t requestedUnitCount,
    std::string& error,
    ProjectOopCodePartitionPolicy policy,
    std::string_view cachedPlan)
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
    const std::vector<std::uint8_t> reachableDefinitions =
        projectOopReachableDefinitionMask(mirModel);
    unitByFunction_.resize(functions.size());
    std::vector<PartitionCandidate> candidates;
    candidates.reserve(functions.size());
    for (std::size_t ordinal = 0; ordinal < functions.size(); ++ordinal)
    {
        const frontend::ProjectOopMirFunction& function = functions[ordinal];
        if (function.isImportedC || function.isImportedNative ||
            ordinal >= reachableDefinitions.size() || reachableDefinitions[ordinal] == 0U) continue;
        candidates.push_back(PartitionCandidate{
            ordinal,
            std::max<std::uint64_t>(1, nodeCost(function.body.get())),
            function.mangledName});
    }

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

    const auto assignBalanced = [&]()
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
                units_.begin(), units_.end(), [](const ProjectOopCodeUnit& left,
                                                const ProjectOopCodeUnit& right) {
                    if (left.estimatedCost != right.estimatedCost)
                        return left.estimatedCost < right.estimatedCost;
                    return left.ordinal < right.ordinal;
                });
            selected->estimatedCost = saturatedAdd(selected->estimatedCost, candidate.cost);
            selected->functionOrdinals.push_back(candidate.functionOrdinal);
        }
    };

    bool restoredCacheAwarePlan = false;

    if (policy == ProjectOopCodePartitionPolicy::StableIncremental)
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
                stableNameHash(candidate.stableName) % unitCount);
            units_[bucket].estimatedCost = saturatedAdd(units_[bucket].estimatedCost, candidate.cost);
            units_[bucket].functionOrdinals.push_back(candidate.functionOrdinal);
        }
        std::erase_if(units_, [](const ProjectOopCodeUnit& unit) {
            return unit.functionOrdinals.empty();
        });
    }
    else if (policy == ProjectOopCodePartitionPolicy::CacheAwareBalanced)
    {
        const std::vector<std::size_t> stableOrder = stableCandidateOrder(candidates);
        persistentFunctionOrdinals_.reserve(stableOrder.size());
        for (const std::size_t index : stableOrder)
            persistentFunctionOrdinals_.push_back(candidates[index].functionOrdinal);
        std::vector<std::uint64_t> historicalCosts;
        std::vector<std::size_t> assignments;
        restoredCacheAwarePlan = parsePersistentPlan(
            cachedPlan, unitCount, candidates.size(), historicalCosts, assignments);
        if (restoredCacheAwarePlan)
        {
            std::vector<std::size_t> functionsPerUnit(unitCount, 0U);
            for (std::size_t index = 0; index < stableOrder.size(); ++index)
            {
                const PartitionCandidate& candidate = candidates[stableOrder[index]];
                ProjectOopCodeUnit& unit = units_[assignments[index]];
                unit.estimatedCost = saturatedAdd(unit.estimatedCost, candidate.cost);
                unit.functionOrdinals.push_back(candidate.functionOrdinal);
                ++functionsPerUnit[assignments[index]];
            }
            if (std::ranges::any_of(functionsPerUnit, [](std::size_t count) { return count == 0U; }))
            {
                restoredCacheAwarePlan = false;
                for (ProjectOopCodeUnit& unit : units_)
                {
                    unit.estimatedCost = 0;
                    unit.functionOrdinals.clear();
                }
            }
            else
            {
                for (std::size_t ordinal = 0; ordinal < unitCount; ++ordinal)
                    units_[ordinal].historicalCost = historicalCosts[ordinal];
            }
        }
        if (!restoredCacheAwarePlan) assignBalanced();
        persistentUnitCount_ = unitCount;
        persistentPlanReady_ = true;
    }
    else
    {
        // Longest-processing-time partitioning gives useful balance while remaining fully stable:
        // cost, physical name and original ordinal are all canonical MIR properties.
        assignBalanced();
    }

    // NSC305-L01. Blend the observed cost into the bounded historical weight (a capped 1:1 moving
    // average). A first observation seeds it directly.
    constexpr std::uint64_t kHistoricalCostCap = std::uint64_t{1} << 24U;
    for (ProjectOopCodeUnit& unit : units_)
    {
        if (!restoredCacheAwarePlan)
        {
            unit.historicalCost = unit.historicalCost == 0U
                ? std::min(unit.estimatedCost, kHistoricalCostCap)
                : boundedHistoricalWeight(
                    unit.historicalCost, unit.estimatedCost, kHistoricalCostCap);
        }
    }

    for (ProjectOopCodeUnit& unit : units_)
    {
        std::sort(unit.functionOrdinals.begin(), unit.functionOrdinals.end());
        for (const std::size_t functionOrdinal : unit.functionOrdinals)
            unitByFunction_[functionOrdinal] = unit.ordinal;
    }

    std::unordered_map<std::string_view, std::vector<std::size_t>> functionsByName;
    std::unordered_map<std::size_t, std::size_t> functionByMember;
    std::unordered_map<std::size_t, std::size_t> destructorFunctionByType;
    std::unordered_map<std::string_view, const frontend::ProjectOopMirStructLayout*> structLayoutByType;
    for (std::size_t ordinal = 0; ordinal < functions.size(); ++ordinal)
    {
        functionsByName[functions[ordinal].mangledName].push_back(ordinal);
        if (!functions[ordinal].isLambda && !functions[ordinal].namespaceFunctionOrdinal.has_value())
            functionByMember.emplace(functions[ordinal].memberOrdinal, ordinal);
        if (functions[ordinal].destructor)
            destructorFunctionByType.emplace(functions[ordinal].declaringTypeOrdinal, ordinal);
    }
    for (const frontend::ProjectOopMirStructLayout& layout : mirModel.structLayouts())
        structLayoutByType.emplace(layout.typeKey, &layout);

    const auto fixedArrayElement = [](std::string_view typeKey) -> std::string_view
    {
        if (typeKey.size() < 3 || typeKey.back() != ']') return {};
        const std::size_t open = typeKey.rfind('[');
        if (open == std::string_view::npos || open == 0 || open + 1 >= typeKey.size() - 1)
            return {};
        for (std::size_t index = open + 1; index + 1 < typeKey.size(); ++index)
            if (typeKey[index] < '0' || typeKey[index] > '9') return {};
        return typeKey.substr(0, open);
    };
    const auto addMember = [&](std::optional<std::size_t> member, auto& dependencies)
    {
        if (!member.has_value()) return;
        const auto found = functionByMember.find(*member);
        if (found != functionByMember.end()) dependencies.insert(found->second);
    };
    const auto collectNode = [&](const frontend::ProjectOopMirNode* root,
                                 auto& dependencies) -> void
    {
        // NSC305-L02. Iterative walk (no recursion in the partition hot path).
        std::vector<const frontend::ProjectOopMirNode*> work;
        if (root != nullptr) work.push_back(root);
        while (!work.empty())
        {
            const frontend::ProjectOopMirNode* node = work.back();
            work.pop_back();
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
            if (node->kind == frontend::ProjectOopMirNodeKind::PollAddress)
                addCallee(node->stringValue);
            if (node->kind == frontend::ProjectOopMirNodeKind::Panic)
                addCallee(mirModel.panicHandlerMangledName());
            addMember(node->memberOrdinal, dependencies);
            addMember(node->secondaryMemberOrdinal, dependencies);
            addMember(node->resultSuccessTestMemberOrdinal, dependencies);
            addMember(node->resultSuccessExtractorMemberOrdinal, dependencies);
            addMember(node->resultErrorExtractorMemberOrdinal, dependencies);
            addMember(node->resultErrorFactoryMemberOrdinal, dependencies);
            addMember(node->resultErrorConversionMemberOrdinal, dependencies);
            addMember(node->disposeImplementationMemberOrdinal, dependencies);
            // NS304-T13-F07: the placement construction references the allocator's Deallocate member
            // for its generated thunk, so that member must be declared in the same code unit.
            addMember(node->allocatorDeallocateMemberOrdinal, dependencies);
            if (node->automaticDestruction)
            {
                std::unordered_set<std::size_t> seen;
                std::vector<std::string_view> typeWork;
                typeWork.push_back(node->typeKey);
                while (!typeWork.empty())
                {
                    const std::string_view typeKey = typeWork.back();
                    typeWork.pop_back();
                    if (const std::string_view element = fixedArrayElement(typeKey);
                        !element.empty())
                    {
                        typeWork.push_back(element);
                        continue;
                    }
                    const auto layoutFound = structLayoutByType.find(typeKey);
                    if (layoutFound == structLayoutByType.end()) continue;
                    const frontend::ProjectOopMirStructLayout& layout = *layoutFound->second;
                    if (!seen.insert(layout.typeOrdinal).second) continue;
                    if (const auto destructor = destructorFunctionByType.find(layout.typeOrdinal);
                        destructor != destructorFunctionByType.end())
                        dependencies.insert(destructor->second);
                    for (const frontend::ObjectFieldLayout& field : layout.fields)
                        typeWork.push_back(field.typeKey);
                }
            }
            for (auto it = node->children.rbegin(); it != node->children.rend(); ++it)
                work.push_back(it->get());
        }
    };
    for (ProjectOopCodeUnit& unit : units_)
    {
        std::unordered_set<std::size_t> dependencies(
            unit.functionOrdinals.begin(), unit.functionOrdinals.end());
        for (const std::size_t functionOrdinal : unit.functionOrdinals)
        {
            if (functionOrdinal >= functions.size()) continue;
            const frontend::ProjectOopMirFunction& function = functions[functionOrdinal];
            collectNode(function.body.get(), dependencies);
            for (const frontend::ProjectOopMirCleanupRegion& cleanup : function.cleanupRegions)
                collectNode(cleanup.action, dependencies);
        }
        unit.declarationOrdinals.assign(dependencies.begin(), dependencies.end());
        std::sort(unit.declarationOrdinals.begin(), unit.declarationOrdinals.end());
    }
    built_ = true;
    return true;
}

std::string ProjectOopCodePartition::persistentPlanKey(
    const frontend::ProjectOopMirModel& mirModel,
    std::size_t requestedUnitCount)
{
    std::vector<std::pair<std::string_view, std::size_t>> identities;
    const std::span<const frontend::ProjectOopMirFunction> functions = mirModel.functions();
    const std::vector<std::uint8_t> reachableDefinitions =
        projectOopReachableDefinitionMask(mirModel);
    identities.reserve(functions.size());
    for (std::size_t ordinal = 0; ordinal < functions.size(); ++ordinal)
    {
        if (functions[ordinal].isImportedC || functions[ordinal].isImportedNative ||
            ordinal >= reachableDefinitions.size() || reachableDefinitions[ordinal] == 0U) continue;
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
