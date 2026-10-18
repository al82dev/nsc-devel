#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nativesharp::frontend
{
class ProjectOopMirModel;
struct ProjectOopMirNode;
}

namespace nativesharp::llvm_backend
{
// NSC305-L06H. Immutable, per-compilation indexes and closures shared by partition planning,
// cache identity preparation and every physical LLVM module. Constructing this view is the only
// operation that walks the complete function catalogue to determine reachable definitions.
class ProjectOopFunctionReachabilityAnalysis final
{
public:
    explicit ProjectOopFunctionReachabilityAnalysis(
        const frontend::ProjectOopMirModel& mirModel);

    std::span<const std::uint8_t> reachableDefinitions() const noexcept;
    std::vector<std::size_t> coordinatorFunctionDeclarations() const;
    std::vector<std::uint8_t> staticFieldDeclarationMask(
        std::span<const std::size_t> definitionOrdinals,
        bool partitioned,
        bool ownsModuleGlobals) const;
    const std::string& projectFingerprint() const noexcept;

private:
    void addReachableFunction(
        std::size_t ordinal, std::vector<std::size_t>& work);
    void addReachableFunctionName(
        std::string_view name, std::vector<std::size_t>& work);
    void addReachableFunctionMember(
        std::size_t member, std::vector<std::size_t>& work);
    void scanReachableFunctionReferences(
        const frontend::ProjectOopMirNode* root, std::vector<std::size_t>& work);
    void addFunctionName(std::string_view name, std::vector<std::uint8_t>& selected) const;
    void addFunctionMember(std::size_t member, std::vector<std::uint8_t>& selected) const;
    void addStaticFieldMember(std::size_t member, std::vector<std::uint8_t>& selected) const;
    void scanFunctionReferences(
        const frontend::ProjectOopMirModel& mirModel,
        const frontend::ProjectOopMirNode* root,
        std::vector<std::uint8_t>& selected) const;
    void scanStaticFieldReferences(
        const frontend::ProjectOopMirNode* root,
        std::vector<std::uint8_t>& selected) const;

    const frontend::ProjectOopMirModel& mirModel_;
    std::vector<std::uint8_t> reachableDefinitions_;
    std::unordered_map<std::string_view, std::vector<std::size_t>> functionsByName_;
    std::unordered_map<std::size_t, std::size_t> functionByMember_;
    std::unordered_map<std::size_t, std::size_t> staticFieldByMember_;
    std::string projectFingerprint_;
};

// Returns one byte per MIR function. A set byte means that the current consumer must emit the
// definition. Source-owned functions are roots; metadata-transported inline bodies enter only
// through the iterative call/member closure of those roots.
std::vector<std::uint8_t> projectOopReachableDefinitionMask(
    const frontend::ProjectOopMirModel& mirModel);

// NSC305-L06F. Selects only static-field declarations required by one physical LLVM module.
// A monolithic module retains the complete model. The partition coordinator owns every local
// field, each ODR-materialized imported constant, and the dependencies of local initializers;
// ordinary units retain fields referenced by the MIR bodies they define.
std::vector<std::uint8_t> projectOopStaticFieldDeclarationMask(
    const frontend::ProjectOopMirModel& mirModel,
    std::span<const std::size_t> definitionOrdinals,
    bool partitioned,
    bool ownsModuleGlobals);

// NSC305-L06F. The coordinator defines globals but no ordinary MIR bodies. It needs declarations
// for local executable symbols used by vtables/wrappers/guards plus callees referenced by local
// field initializers, not the complete imported API catalogue.
std::vector<std::size_t> projectOopCoordinatorFunctionDeclarations(
    const frontend::ProjectOopMirModel& mirModel);
}
