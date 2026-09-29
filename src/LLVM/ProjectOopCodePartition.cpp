#include "nativesharp/LLVM/ProjectOopCodePartition.h"

#include "nativesharp/Frontend/ProjectOopMirModel.h"

#include <algorithm>
#include <iomanip>
#include <limits>
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
    std::uint64_t cost = 1;
    for (const auto& child : node->children)
        cost = saturatedAdd(cost, nodeCost(child.get()));
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
}

bool ProjectOopCodePartition::build(
    const frontend::ProjectOopMirModel& mirModel,
    std::size_t requestedUnitCount,
    std::string& error,
    ProjectOopCodePartitionPolicy policy)
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

    struct Candidate final
    {
        std::size_t functionOrdinal = 0;
        std::uint64_t cost = 0;
        std::string_view stableName;
    };

    const std::span<const frontend::ProjectOopMirFunction> functions = mirModel.functions();
    unitByFunction_.resize(functions.size());
    std::vector<Candidate> candidates;
    candidates.reserve(functions.size());
    for (std::size_t ordinal = 0; ordinal < functions.size(); ++ordinal)
    {
        const frontend::ProjectOopMirFunction& function = functions[ordinal];
        if (function.isImportedC || function.isImportedNative) continue;
        candidates.push_back(Candidate{
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
        units_.push_back(ProjectOopCodeUnit{ordinal, codeUnitKey(ordinal), 0, {}, {}});

    if (policy == ProjectOopCodePartitionPolicy::StableIncremental)
    {
        // Identity hashing keeps a body-only edit in the same bucket. Empty buckets are omitted
        // without renumbering surviving keys, so unrelated object-cache identities stay stable.
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& left, const Candidate& right) {
            if (left.stableName != right.stableName) return left.stableName < right.stableName;
            return left.functionOrdinal < right.functionOrdinal;
        });
        for (const Candidate& candidate : candidates)
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
    else
    {
        // Longest-processing-time partitioning gives useful balance while remaining fully stable:
        // cost, physical name and original ordinal are all canonical MIR properties.
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& left, const Candidate& right) {
            if (left.cost != right.cost) return left.cost > right.cost;
            if (left.stableName != right.stableName) return left.stableName < right.stableName;
            return left.functionOrdinal < right.functionOrdinal;
        });
        for (const Candidate& candidate : candidates)
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
    }

    for (ProjectOopCodeUnit& unit : units_)
    {
        std::sort(unit.functionOrdinals.begin(), unit.functionOrdinals.end());
        for (const std::size_t functionOrdinal : unit.functionOrdinals)
            unitByFunction_[functionOrdinal] = unit.ordinal;
    }

    std::unordered_map<std::string_view, std::vector<std::size_t>> functionsByName;
    std::unordered_map<std::size_t, std::size_t> functionByMember;
    for (std::size_t ordinal = 0; ordinal < functions.size(); ++ordinal)
    {
        functionsByName[functions[ordinal].mangledName].push_back(ordinal);
        if (!functions[ordinal].isLambda && !functions[ordinal].namespaceFunctionOrdinal.has_value())
            functionByMember.emplace(functions[ordinal].memberOrdinal, ordinal);
    }
    const auto addMember = [&](std::optional<std::size_t> member, auto& dependencies)
    {
        if (!member.has_value()) return;
        const auto found = functionByMember.find(*member);
        if (found != functionByMember.end()) dependencies.insert(found->second);
    };
    const auto collectNode = [&](const auto& self, const frontend::ProjectOopMirNode* node,
                                 auto& dependencies) -> void
    {
        if (node == nullptr) return;
        if (!node->calleeName.empty())
        {
            const auto found = functionsByName.find(node->calleeName);
            if (found != functionsByName.end())
                dependencies.insert(found->second.begin(), found->second.end());
        }
        addMember(node->memberOrdinal, dependencies);
        addMember(node->secondaryMemberOrdinal, dependencies);
        addMember(node->disposeImplementationMemberOrdinal, dependencies);
        for (const auto& child : node->children) self(self, child.get(), dependencies);
    };
    for (ProjectOopCodeUnit& unit : units_)
    {
        std::unordered_set<std::size_t> dependencies(
            unit.functionOrdinals.begin(), unit.functionOrdinals.end());
        for (const std::size_t functionOrdinal : unit.functionOrdinals)
        {
            if (functionOrdinal >= functions.size()) continue;
            const frontend::ProjectOopMirFunction& function = functions[functionOrdinal];
            collectNode(collectNode, function.body.get(), dependencies);
            for (const frontend::ProjectOopMirCleanupRegion& cleanup : function.cleanupRegions)
                collectNode(collectNode, cleanup.action, dependencies);
        }
        unit.declarationOrdinals.assign(dependencies.begin(), dependencies.end());
        std::sort(unit.declarationOrdinals.begin(), unit.declarationOrdinals.end());
    }
    built_ = true;
    return true;
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
