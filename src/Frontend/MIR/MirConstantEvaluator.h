#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
struct ProjectOopHirNode;
struct ProjectOopMirStaticField;

class MirConstantEvaluator final
{
public:
    static std::optional<std::uint64_t> integerValue(
        const ProjectOopHirNode* node,
        const std::vector<ProjectOopMirStaticField>& knownFields) noexcept;
    static std::optional<double> floatingValue(
        const ProjectOopHirNode* node,
        const std::vector<ProjectOopMirStaticField>& knownFields) noexcept;
    static std::optional<bool> booleanValue(
        const ProjectOopHirNode* node,
        const std::vector<ProjectOopMirStaticField>& knownFields) noexcept;
    static std::optional<std::string> stringValue(
        const ProjectOopHirNode* node,
        const std::vector<ProjectOopMirStaticField>& knownFields);
};
}
