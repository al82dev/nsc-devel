#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"

#include <cstddef>
#include <memory>
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
enum class ConstructionResolutionKind
{
    ResolvedExplicit,
    ResolvedImplicitDefault,
    NoViableCandidate,
    Ambiguous,
    Inaccessible,
    InvalidTarget
};

struct ConstructorCandidateBinding final
{
    std::optional<std::size_t> memberOrdinal;
    bool implicitDefault = false;
    bool accessible = false;
    bool viable = false;
    std::vector<ProjectImplicitConversion> conversions;
};


enum class ConstructorInitializerKind
{
    ImplicitBase,
    ExplicitBase,
    ExplicitThis
};

enum class ConstructorInitializerResolutionKind
{
    NoChainingRequired,
    ResolvedExplicit,
    ResolvedImplicitDefault,
    NoViableCandidate,
    Ambiguous,
    Inaccessible,
    Invalid
};

struct ConstructorInitializerBinding final
{
    ConstructorInitializerResolutionKind resolution = ConstructorInitializerResolutionKind::Invalid;
    ConstructorInitializerKind kind = ConstructorInitializerKind::ImplicitBase;
    const ast::AstNode* initializerSyntax = nullptr;
    std::size_t constructorMemberOrdinal = 0;
    std::size_t containingTypeOrdinal = 0;
    std::optional<std::size_t> targetTypeOrdinal;
    bool explicitInitializer = false;
    std::vector<std::size_t> explicitConstructorMemberOrdinals;
    std::vector<ConstructorCandidateBinding> evaluatedCandidates;
    std::optional<std::size_t> selectedConstructorMemberOrdinal;
    std::optional<std::size_t> selectedEntryConstructorMemberOrdinal;
    bool selectedImplicitDefault = false;
    std::vector<ProjectImplicitConversion> selectedConversions;
};

enum class ImplicitDefaultConstructorResolutionKind
{
    NoBaseRequired,
    ResolvedExplicitBaseEntry,
    ResolvedImplicitBaseChain,
    NoViableBaseConstructor,
    InaccessibleBaseConstructor,
    Invalid
};

struct ImplicitDefaultConstructorBinding final
{
    ImplicitDefaultConstructorResolutionKind resolution =
        ImplicitDefaultConstructorResolutionKind::Invalid;
    std::size_t typeOrdinal = 0;
    std::optional<std::size_t> baseTypeOrdinal;
    std::optional<std::size_t> directBaseConstructorMemberOrdinal;
    std::optional<std::size_t> entryConstructorMemberOrdinal;
};

struct ObjectConstructionBinding final
{
    ConstructionResolutionKind resolution = ConstructionResolutionKind::InvalidTarget;
    const ast::AstNode* syntax = nullptr;
    std::size_t callerMemberOrdinal = 0;
    std::size_t targetTypeOrdinal = 0;
    // C6 "struct members": `new Vec2(1, 2)` on a STRUCT means "initialize in place", not
    // "allocate". Everything about resolving which constructor runs is identical to a class --
    // same candidates, same overload rules, same argument conversions -- so only this flag and
    // the lowering differ. See ProjectOopHirModel's ObjectCreationExpression case.
    bool targetIsStruct = false;
    std::string targetTypeKey;
    std::vector<std::size_t> explicitConstructorMemberOrdinals;
    std::vector<ConstructorCandidateBinding> evaluatedCandidates;
    std::optional<std::size_t> selectedConstructorMemberOrdinal;
    bool selectedImplicitDefault = false;
    std::optional<std::size_t> implicitDefaultBaseConstructorMemberOrdinal;
    std::vector<ProjectImplicitConversion> selectedConversions;
};

class ProjectConstructionModel final
{
public:
    bool build(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectInterfaceModel& interfaceModel,
        const ProjectMethodBodyModel& methodBodyModel,
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics);

    std::span<const ObjectConstructionBinding> constructions() const noexcept;
    std::span<const ConstructorInitializerBinding> constructorInitializerBindings() const noexcept;
    const ConstructorInitializerBinding* constructorInitializerBindingForMember(std::size_t memberOrdinal) const noexcept;
    std::span<const ImplicitDefaultConstructorBinding> implicitDefaultConstructorBindings() const noexcept;
    const ImplicitDefaultConstructorBinding* implicitDefaultConstructorBindingForType(
        std::size_t typeOrdinal) const noexcept;
    bool isBuilt() const noexcept;

private:
    void scanUnsupportedContexts(
        const ast::AstNode* node,
        diagnostics::DiagnosticEngine& diagnostics);
    void scanNode(
        const ast::AstNode* node,
        std::size_t callerMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void bindImplicitDefaultConstructors(diagnostics::DiagnosticEngine& diagnostics);
    void bindImplicitDefaultConstructor(
        std::size_t typeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void bindConstructorInitializer(
        std::size_t constructorMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void validateThisConstructorCycles(diagnostics::DiagnosticEngine& diagnostics);
    void bindConstruction(
        const ast::AstNode* expression,
        std::size_t callerMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    std::optional<MethodExpressionTypeBinding> expressionType(const ast::AstNode* syntax) const;
    bool constructorAccessible(
        const ClassMemberSymbol& constructor,
        std::size_t callerMemberOrdinal,
        bool baseInitializer) const noexcept;
    bool constructorAccessibleFromType(
        const ClassMemberSymbol& constructor,
        std::size_t callerTypeOrdinal,
        bool baseInitializer) const noexcept;
    bool implicitDefaultAccessible(
        std::size_t targetTypeOrdinal,
        std::size_t callerMemberOrdinal,
        bool baseInitializer) const noexcept;
    bool implicitDefaultAccessibleFromType(
        std::size_t targetTypeOrdinal,
        std::size_t callerTypeOrdinal,
        bool baseInitializer) const noexcept;
    bool betterCandidate(
        const ConstructorCandidateBinding& left,
        const ConstructorCandidateBinding& right) const noexcept;

    const ProjectNameLookup* nameLookup_ = nullptr;
    const ProjectMemberModel* memberModel_ = nullptr;
    const ProjectInheritanceModel* inheritanceModel_ = nullptr;
    const ProjectInterfaceModel* interfaceModel_ = nullptr;
    const ProjectMethodBodyModel* methodBodyModel_ = nullptr;
    std::vector<ObjectConstructionBinding> constructions_;
    std::vector<ConstructorInitializerBinding> constructorInitializerBindings_;
    std::vector<std::optional<std::size_t>> initializerBindingByMember_;
    std::vector<ImplicitDefaultConstructorBinding> implicitDefaultConstructorBindings_;
    std::vector<std::optional<std::size_t>> implicitDefaultBindingByType_;
    bool built_ = false;
};

const char* constructionResolutionKindName(ConstructionResolutionKind kind) noexcept;
const char* constructorInitializerKindName(ConstructorInitializerKind kind) noexcept;
const char* constructorInitializerResolutionKindName(ConstructorInitializerResolutionKind kind) noexcept;
const char* implicitDefaultConstructorResolutionKindName(
    ImplicitDefaultConstructorResolutionKind kind) noexcept;
}
