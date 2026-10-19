#pragma once

#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/InterfaceTypeInfo.h"
#include "nativesharp/Frontend/ClassInterfaceTablePlan.h"
#include "nativesharp/Frontend/TraitOperatorWitness.h"
#include "nativesharp/Frontend/TraitObjectContract.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "Internal/ProjectInterfaceDirectSource.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
class ProjectInterfaceModel final
{
public:
    bool build(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInheritanceModel& inheritanceModel,
        diagnostics::DiagnosticEngine& diagnostics);

    std::span<const InterfaceTypeInfo> interfaces() const noexcept;
    const InterfaceTypeInfo* interfaceInfo(std::size_t typeOrdinal) const noexcept;
    bool isInterface(std::size_t typeOrdinal) const noexcept;
    bool interfaceDerivesFrom(
        std::size_t derivedInterfaceOrdinal,
        std::size_t baseInterfaceOrdinal,
        bool includeIdentity = false) const noexcept;
    bool classImplementsInterface(
        std::size_t classTypeOrdinal,
        std::size_t interfaceTypeOrdinal) const noexcept;
    std::span<const std::size_t> interfacesOfClass(std::size_t classTypeOrdinal) const noexcept;
    std::span<const TraitOperatorWitness> operatorWitnessesOfType(
        std::size_t typeOrdinal) const noexcept;
    std::span<const ClassInterfaceTablePlan> tablePlans() const noexcept;
    const ClassInterfaceTablePlan* tablePlan(
        std::size_t classTypeOrdinal,
        std::size_t interfaceTypeOrdinal) const noexcept;
    const TraitObjectContract* traitObjectContract(
        std::size_t traitTypeOrdinal) const noexcept;
    std::optional<std::size_t> typeOrdinalForTypeKey(std::string_view typeKey) const noexcept;
    std::string typeKeyForOrdinal(std::size_t typeOrdinal) const;
    bool isBuilt() const noexcept;

private:
    enum class VisitState
    {
        Unvisited,
        Visiting,
        Complete,
        Failed
    };

    bool collectInterfaceBases(diagnostics::DiagnosticEngine& diagnostics);
    bool visitInterface(
        std::size_t typeOrdinal,
        std::vector<VisitState>& states,
        diagnostics::DiagnosticEngine& diagnostics);
    bool inheritInterfaceMethod(
        std::size_t typeOrdinal,
        InterfaceTypeInfo& info,
        std::size_t memberOrdinal,
        std::unordered_map<std::string, std::size_t>& signatureOwner,
        std::unordered_map<std::string, std::vector<std::size_t>>& inheritedDefaults,
        diagnostics::DiagnosticEngine& diagnostics);
    bool inheritStaticRequirement(
        std::size_t typeOrdinal,
        InterfaceTypeInfo& info,
        std::size_t memberOrdinal,
        std::unordered_map<std::string, std::size_t>& staticSignatureOwner,
        diagnostics::DiagnosticEngine& diagnostics);
    bool inheritBaseInterface(
        std::size_t typeOrdinal,
        InterfaceTypeInfo& info,
        std::size_t baseOrdinal,
        std::vector<VisitState>& states,
        std::unordered_map<std::string, std::size_t>& signatureOwner,
        std::unordered_map<std::string, std::size_t>& staticSignatureOwner,
        std::unordered_map<std::string, std::vector<std::size_t>>& inheritedDefaults,
        diagnostics::DiagnosticEngine& diagnostics);
    bool collectInheritedInterfaceMembers(
        std::size_t typeOrdinal,
        InterfaceTypeInfo& info,
        std::vector<VisitState>& states,
        std::unordered_map<std::string, std::size_t>& signatureOwner,
        std::unordered_map<std::string, std::size_t>& staticSignatureOwner,
        std::unordered_map<std::string, std::vector<std::size_t>>& inheritedDefaults,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateDeclaredInterfaceMethod(
        const InterfaceTypeInfo& info,
        const ClassMemberSymbol& method,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool mergeTraitStaticRequirement(
        InterfaceTypeInfo& info,
        std::size_t memberOrdinal,
        std::unordered_map<std::string, std::size_t>& staticSignatureOwner,
        diagnostics::DiagnosticEngine& diagnostics);
    bool mergeDeclaredInterfaceMethod(
        InterfaceTypeInfo& info,
        std::size_t memberOrdinal,
        std::unordered_map<std::string, std::size_t>& signatureOwner,
        std::unordered_set<std::string>& redeclaredSignatures,
        diagnostics::DiagnosticEngine& diagnostics);
    bool collectDeclaredInterfaceMembers(
        std::size_t typeOrdinal,
        InterfaceTypeInfo& info,
        std::unordered_map<std::string, std::size_t>& signatureOwner,
        std::unordered_map<std::string, std::size_t>& staticSignatureOwner,
        std::unordered_set<std::string>& redeclaredSignatures,
        diagnostics::DiagnosticEngine& diagnostics);
    bool inheritedDefaultIsDominated(
        const std::vector<std::size_t>& defaults,
        std::size_t candidateOrdinal) const;
    bool validateInheritedDefaults(
        std::size_t typeOrdinal,
        const std::unordered_map<std::string, std::vector<std::size_t>>& inheritedDefaults,
        const std::unordered_set<std::string>& redeclaredSignatures,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool collectDirectInterfaces(
        std::size_t classOrdinal,
        bool valueType,
        std::vector<std::size_t>& implemented,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateNonInterfaceSpecification(
        std::size_t candidateOrdinal,
        NamedTypeDeclarationKind kind,
        bool valueType,
        bool sawInterface,
        const ast::TypeSyntax& specification,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool validateDirectInterfaceTarget(
        std::size_t candidateOrdinal,
        bool valueType,
        bool explicitImpl,
        const InterfaceTypeInfo* info,
        const ast::TypeSyntax& specification,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool validateDirectInterfaceDuplicate(
        std::size_t classOrdinal,
        std::size_t candidateOrdinal,
        bool explicitImpl,
        const ast::TypeSyntax& specification,
        const std::unordered_map<std::size_t, ProjectInterfaceDirectSource>& directInterfaces,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool collectDirectInterface(
        std::size_t classOrdinal,
        bool valueType,
        const ast::TypeSyntax& specification,
        bool& sawInterface,
        std::unordered_map<std::size_t, ProjectInterfaceDirectSource>& directInterfaces,
        std::vector<std::size_t>& implemented,
        diagnostics::DiagnosticEngine& diagnostics);
    bool implementationThrowsCompatible(
        const ClassMemberSymbol& implementation,
        const ClassMemberSymbol& requirement) const;
    bool appendInterfaceMethodImplementation(
        std::size_t classOrdinal,
        std::size_t interfaceOrdinal,
        std::size_t interfaceMethodOrdinal,
        const NamedTypeDeclaration& type,
        const ClassMemberSymbol& interfaceMethod,
        ClassInterfaceTablePlan& plan,
        diagnostics::DiagnosticEngine& diagnostics);
    void appendOperatorWitness(
        std::size_t classOrdinal,
        std::size_t interfaceOrdinal,
        std::size_t requirementOrdinal,
        std::size_t implementationOrdinal,
        const ClassMemberSymbol& requirement);
    bool validateStaticRequirementImplementation(
        std::size_t classOrdinal,
        std::size_t interfaceOrdinal,
        const NamedTypeDeclaration& type,
        std::size_t requirementOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool buildInterfacePlan(
        std::size_t classOrdinal,
        bool valueType,
        std::size_t interfaceOrdinal,
        const NamedTypeDeclaration& type,
        diagnostics::DiagnosticEngine& diagnostics);
    bool buildImplementedInterfacePlans(
        std::size_t classOrdinal,
        bool valueType,
        const std::vector<std::size_t>& implemented,
        diagnostics::DiagnosticEngine& diagnostics);
    bool buildImplementingTypePlans(
        std::size_t classOrdinal,
        std::optional<std::size_t> baseOrdinal,
        bool valueType,
        diagnostics::DiagnosticEngine& diagnostics);
    bool buildClassPlans(diagnostics::DiagnosticEngine& diagnostics);
    bool buildTraitObjectContracts(diagnostics::DiagnosticEngine& diagnostics);
    std::optional<std::size_t> findImplementation(
        std::size_t classTypeOrdinal,
        const ClassMemberSymbol& interfaceMethod) const noexcept;
    std::optional<std::size_t> findStaticImplementation(
        std::size_t classTypeOrdinal,
        const ClassMemberSymbol& requirement) const noexcept;
    static std::uint64_t stableTypeId(std::string_view value) noexcept;

    const ProjectNameLookup* nameLookup_ = nullptr;
    const ProjectMemberModel* memberModel_ = nullptr;
    const ProjectInheritanceModel* inheritanceModel_ = nullptr;
    std::vector<InterfaceTypeInfo> interfaces_;
    std::vector<std::optional<std::size_t>> interfaceInfoByType_;
    std::vector<std::vector<std::size_t>> interfacesByClass_;
    std::vector<std::vector<TraitOperatorWitness>> operatorWitnessesByType_;
    std::vector<ClassInterfaceTablePlan> tablePlans_;
    std::vector<TraitObjectContract> traitObjectContracts_;
    bool built_ = false;
};
}
