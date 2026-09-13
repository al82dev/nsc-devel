#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
class ProjectOopMirModel;
}

namespace nativesharp::llvm_backend
{
struct ProjectOopCodeUnit final
{
    std::size_t ordinal = 0;
    std::string stableKey;
    std::uint64_t estimatedCost = 0;
    std::vector<std::size_t> functionOrdinals;
};

class ProjectOopCodePartition final
{
public:
    bool build(
        const frontend::ProjectOopMirModel& mirModel,
        std::size_t requestedUnitCount,
        std::string& error);

    std::span<const ProjectOopCodeUnit> units() const noexcept;
    std::optional<std::size_t> unitForFunction(std::size_t functionOrdinal) const noexcept;
    std::string dump() const;
    bool isBuilt() const noexcept;

private:
    std::vector<ProjectOopCodeUnit> units_;
    std::vector<std::optional<std::size_t>> unitByFunction_;
    bool built_ = false;
};
}
