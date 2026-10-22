#pragma once

#include "nativesharp/Frontend/ObjectFieldLayout.h"
#include "nativesharp/Frontend/StructValueLayout.h"
#include "nativesharp/Frontend/ClassObjectLayout.h"
#include "nativesharp/Frontend/ObjectInitializationStepKind.h"
#include "nativesharp/Frontend/ObjectInitializationStep.h"
#include "nativesharp/Frontend/ObjectAllocationPlan.h"
#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Language/AggregateSemantics.h"
#include "nativesharp/Target/TargetDataLayout.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
struct ProjectOopHirModelTestAccess;








class ProjectObjectLayoutModel final
{
public:
    ProjectObjectLayoutModel() noexcept;
    explicit ProjectObjectLayoutModel(ProjectCompilationIndexes& indexes) noexcept;

    bool build(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectConstructionModel& constructionModel,
        diagnostics::DiagnosticEngine& diagnostics);

    std::span<const ClassObjectLayout> classLayouts() const noexcept;
    std::span<const StructValueLayout> structLayouts() const noexcept;
    std::span<const ObjectAllocationPlan> allocationPlans() const noexcept;
    const ClassObjectLayout* layoutForType(std::size_t typeOrdinal) const noexcept;
    const StructValueLayout* structLayoutForType(std::size_t typeOrdinal) const noexcept;
    const ObjectFieldLayout* fieldLayoutForMember(std::size_t memberOrdinal) const noexcept;
    target::TypeLayout classReferenceLayout() const noexcept;
    // The size and alignment of ONE value of `typeKey` -- what an array stride, a `sizeof` and an
    // aggregate-element offset all need. Builtins answer from the target's fixed widths, a struct
    // from its computed value layout, and everything reference-shaped (a class, an interface, a
    // dynamic array, a pointer, a string) from the pointer width. nullopt for a type key this
    // project does not know. Shared by the MIR model and the OOP HIR model so a stride and an
    // offset can never be computed by two different rules.
    std::optional<std::pair<std::uint64_t, std::uint64_t>> valueLayoutForTypeKey(
        const ProjectNameLookup& nameLookup,
        std::string_view typeKey) const;
    std::optional<language::AggregateLayout> tupleLayoutForTypeKey(
        const ProjectNameLookup& nameLookup,
        std::string_view typeKey) const;
    bool isBuilt() const noexcept;

private:
    friend struct ProjectOopHirModelTestAccess;

    enum class NamedLayoutState
    {
        Unvisited,
        Visiting,
        Complete,
        Failed
    };

    std::optional<target::TypeLayout> layoutTypeSyntax(
        const ast::TypeSyntax* syntax,
        diagnostics::DiagnosticEngine& diagnostics,
        SourceRange useRange);
    std::optional<target::TypeLayout> layoutBaseType(
        const ast::TypeSyntax* syntax,
        diagnostics::DiagnosticEngine& diagnostics,
        SourceRange useRange);
    std::optional<target::TypeLayout> layoutNamedType(
        std::size_t typeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        SourceRange useRange);
    std::optional<std::uint64_t> evaluateArrayCount(const ast::AstNode* expression) const noexcept;
    bool buildStructLayout(
        std::size_t typeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics,
        SourceRange useRange);
    bool hasTrivialStructLifetime(
        std::string_view typeKey,
        std::set<std::size_t>& visiting) const;
    bool structBitFieldWidth(
        const ClassMemberSymbol& member,
        const target::TypeLayout& fieldLayout,
        std::size_t typeOrdinal,
        std::optional<std::uint64_t>& bitWidth,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool validateFlexibleStructField(
        const ClassMemberSymbol& member,
        std::string_view elementTypeKey,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool collectStructPhysicalFields(
        std::size_t typeOrdinal,
        std::vector<target::StructFieldRequest>& physicalFields,
        std::vector<std::size_t>& fieldMembers,
        diagnostics::DiagnosticEngine& diagnostics);
    bool validateTransparentStruct(
        std::span<const target::StructFieldRequest> physicalFields,
        std::span<const std::size_t> fieldMembers,
        const target::StructLayoutOptions& layoutOptions,
        diagnostics::DiagnosticEngine& diagnostics,
        SourceRange useRange) const;
    bool validateFlexibleStructPlacement(
        std::span<const target::StructFieldRequest> physicalFields,
        std::span<const std::size_t> fieldMembers,
        const target::StructLayoutOptions& layoutOptions,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool structFieldTransitivelyOwnsStorage(
        std::size_t index,
        std::span<const std::size_t> fieldMembers) const;
    bool validateStructFieldOverlaps(
        const target::StructLayout& layout,
        std::span<const std::size_t> fieldMembers,
        diagnostics::DiagnosticEngine& diagnostics) const;
    StructValueLayout makeStructValueLayout(
        std::size_t typeOrdinal,
        const NamedTypeDeclaration& type,
        const target::StructLayoutOptions& layoutOptions,
        const target::StructLayout& layout,
        std::span<const std::size_t> fieldMembers,
        bool hasExplicitFieldOffsets) const;
    bool validateStructAssertedSize(
        std::size_t typeOrdinal,
        const NamedTypeDeclaration& type,
        const target::StructLayout& layout,
        diagnostics::DiagnosticEngine& diagnostics);
    bool buildClassLayout(
        std::size_t typeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool prepareClassBaseLayout(
        const NamedTypeDeclaration& type,
        const std::optional<std::size_t>& baseType,
        std::uint64_t& offset,
        std::uint64_t& aggregateAlignment,
        std::uint64_t& baseSubobjectSize,
        diagnostics::DiagnosticEngine& diagnostics);
    static ClassObjectLayout makeClassObjectLayout(
        std::size_t typeOrdinal,
        const NamedTypeDeclaration& type,
        const target::TypeLayout& reference,
        const std::optional<std::size_t>& baseType,
        std::uint64_t baseSubobjectSize);
    bool isClassBitFieldBase(std::string_view typeKey) const;
    bool classBitFieldWidth(
        const ClassMemberSymbol& member,
        const target::TypeLayout& fieldLayout,
        std::size_t typeOrdinal,
        std::optional<std::uint64_t>& bitWidth,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool collectClassPhysicalFields(
        std::size_t typeOrdinal,
        std::vector<target::StructFieldRequest>& physicalFields,
        std::vector<std::size_t>& fieldMembers,
        diagnostics::DiagnosticEngine& diagnostics);
    bool finalizeClassLayout(
        std::size_t typeOrdinal,
        const NamedTypeDeclaration& type,
        std::uint64_t minimumSize,
        const std::vector<target::StructFieldRequest>& physicalFields,
        const std::vector<std::size_t>& fieldMembers,
        ClassObjectLayout& layout,
        diagnostics::DiagnosticEngine& diagnostics);
    bool buildPromotedFieldLayouts(diagnostics::DiagnosticEngine& diagnostics);
    void buildAllocationPlans();

    ProjectCompilationIndexes ownedIndexes_;
    ProjectCompilationIndexes* indexes_ = nullptr;
    const ProjectNameLookup* nameLookup_ = nullptr;
    const ProjectMemberModel* memberModel_ = nullptr;
    const ProjectInheritanceModel* inheritanceModel_ = nullptr;
    const ProjectConstructionModel* constructionModel_ = nullptr;
    target::TargetDataLayout targetLayout_ = target::TargetDataLayout::WindowsX64();
    std::vector<StructValueLayout> structLayouts_;
    std::vector<std::optional<std::size_t>> structLayoutIndexByType_;
    std::vector<ClassObjectLayout> classLayouts_;
    std::vector<ObjectFieldLayout> promotedFieldLayouts_;
    std::vector<std::optional<std::size_t>> classLayoutIndexByType_;
    std::vector<target::TypeLayout> namedTypeLayouts_;
    std::vector<NamedLayoutState> namedLayoutStates_;
    std::vector<ObjectAllocationPlan> allocationPlans_;
    bool built_ = false;
};

const char* objectInitializationStepKindName(ObjectInitializationStepKind kind) noexcept;
}
