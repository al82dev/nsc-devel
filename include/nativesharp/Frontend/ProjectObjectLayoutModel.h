#pragma once

#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Language/AggregateSemantics.h"
#include "nativesharp/Target/TargetDataLayout.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
struct ObjectFieldLayout final
{
    std::size_t memberOrdinal = 0;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::uint64_t alignment = 1;
    std::string typeKey;
    std::optional<std::uint64_t> bitOffset;
    std::optional<std::uint64_t> bitWidth;
};


struct StructValueLayout final
{
    std::size_t typeOrdinal = 0;
    std::string typeKey;
    std::uint64_t size = 0;
    std::uint64_t alignment = 1;
    // C2.2 "union and explicit layout": what the declaration asked for, carried alongside the
    // computed result so consumers that must REPRODUCE the layout in another language -- the C
    // header generator above all -- can emit `union` / `#pragma pack(N)` / an alignment attribute
    // instead of guessing from offsets that happen to coincide.
    bool isUnion = false;
    std::uint64_t pack = 0;
    std::uint64_t minimumAlignment = 0;
    bool hasExplicitFieldOffsets = false;
    std::vector<ObjectFieldLayout> fields;
};

struct ClassObjectLayout final
{
    std::size_t typeOrdinal = 0;
    std::string typeKey;
    std::uint64_t size = 0;
    std::uint64_t alignment = 1;
    std::uint64_t headerSize = 0;
    std::optional<std::size_t> baseTypeOrdinal;
    std::uint64_t baseSubobjectSize = 0;
    std::uint64_t referenceSize = 0;
    std::uint64_t referenceAlignment = 1;
    std::vector<ObjectFieldLayout> fields;
};

enum class ObjectInitializationStepKind
{
    RequestStorage,
    CheckAllocationResult,
    BeginObjectLifetime,
    ZeroInitializeStorage,
    InitializeObjectInfrastructure,
    InvokeConstructor,
    ReturnReference
};

struct ObjectInitializationStep final
{
    ObjectInitializationStepKind kind = ObjectInitializationStepKind::RequestStorage;
    std::optional<std::size_t> constructorMemberOrdinal;
};

struct ObjectAllocationPlan final
{
    std::size_t constructionOrdinal = 0;
    std::size_t targetTypeOrdinal = 0;
    std::string targetTypeKey;
    std::uint64_t objectSize = 0;
    std::uint64_t objectAlignment = 1;
    std::uint64_t referenceSize = 0;
    std::uint64_t referenceAlignment = 1;
    bool allocationFailureReturnsNull = true;
    bool zeroInitializesStorage = true;
    bool selectedImplicitDefault = false;
    std::optional<std::size_t> selectedConstructorMemberOrdinal;
    std::optional<std::size_t> constructorEntryMemberOrdinal;
    std::vector<ObjectInitializationStep> initializationSteps;
};

class ProjectObjectLayoutModel final
{
public:
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
    bool buildClassLayout(
        std::size_t typeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool buildPromotedFieldLayouts(diagnostics::DiagnosticEngine& diagnostics);
    void buildAllocationPlans();

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
