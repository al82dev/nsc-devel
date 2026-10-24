#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Frontend/ConstructionResolutionKind.h"
#include "nativesharp/Frontend/ConstructorCandidateBinding.h"
#include "nativesharp/Frontend/ConstructorInitializerBinding.h"
#include "nativesharp/Frontend/ConstructorInitializerKind.h"
#include "nativesharp/Frontend/ConstructorInitializerResolutionKind.h"
#include "nativesharp/Frontend/ConstructorOwnershipTransfer.h"
#include "nativesharp/Frontend/ImplicitDefaultConstructorBinding.h"
#include "nativesharp/Frontend/ImplicitDefaultConstructorResolutionKind.h"
#include "nativesharp/Frontend/ObjectConstructionBinding.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectMethodBodyModel.h"
#include "nativesharp/Frontend/ProjectConstructionStatistics.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"
#include "nativesharp/Frontend/ProjectSyntaxUnit.h"
#include "nativesharp/Frontend/PrecompiledSnapshotUnit.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
struct ProjectOopHirModelTestAccess;
class PrecompiledSnapshotReader;

class ProjectConstructionModel final
{
public:
    bool build(
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectInterfaceModel& interfaceModel,
        const ProjectMethodBodyModel& methodBodyModel,
        const ProjectNamespaceFunctionModel& namespaceFunctionModel,
        std::span<const std::unique_ptr<ProjectSyntaxUnit>> units,
        diagnostics::DiagnosticEngine& diagnostics);

    std::span<const ObjectConstructionBinding> constructions() const noexcept;
    std::span<const ConstructorInitializerBinding> constructorInitializerBindings() const noexcept;
    const ConstructorInitializerBinding* constructorInitializerBindingForMember(std::size_t memberOrdinal) const noexcept;
    std::span<const ImplicitDefaultConstructorBinding> implicitDefaultConstructorBindings() const noexcept;
    const ImplicitDefaultConstructorBinding* implicitDefaultConstructorBindingForType(
        std::size_t typeOrdinal) const noexcept;
    const ProjectConstructionStatistics& statistics() const noexcept;
    bool isBuilt() const noexcept;

    // EN: Plan item 3.11. Accepted snapshots, set before build(). A class whose parts are all in a
    //     snapshot unit takes its implicit default-constructor binding from the `construction`
    //     section at the point where it is bound today; its base type still comes from the
    //     inheritance model.
    // ES: Punto 3.11 del plan. Snapshots aceptados, asignados antes de build(). Una clase con todas
    //     sus partes en una unidad con snapshot toma su binding de constructor por defecto implícito
    //     de la sección `construction` en el punto donde hoy se enlaza; su tipo base sigue viniendo
    //     del modelo de herencia.
    void setPrecompiledSnapshots(std::span<const PrecompiledSnapshotUnit> snapshots) noexcept;
    std::size_t installedSnapshotClassCount() const noexcept;
    // EN: Producer side: writes the bindings of the classes whose parts are all in `fileId`.
    // ES: Lado productor: escribe los bindings de las clases con todas sus partes en `fileId`.
    void setPrecompiledSnapshotCaptureFile(FileId fileId) noexcept;
    bool writePrecompiledSnapshotConstruction(std::string& payload, std::string& error) const;

private:
    friend struct ProjectOopHirModelTestAccess;

    void scanUnsupportedContexts(
        const ast::AstNode* node,
        diagnostics::DiagnosticEngine& diagnostics);
    void bindImplicitDefaultConstructors(diagnostics::DiagnosticEngine& diagnostics);
    void bindImplicitDefaultConstructor(
        std::size_t typeOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    void readSnapshotConstruction();
    bool readSnapshotImplicitDefault(PrecompiledSnapshotReader& reader);
    bool installSnapshotImplicitDefault(std::size_t typeOrdinal);
    void bindExplicitBaseDefaultConstructor(
        std::size_t typeOrdinal,
        std::span<const std::size_t> baseConstructors,
        ImplicitDefaultConstructorBinding& binding,
        diagnostics::DiagnosticEngine& diagnostics);
    void bindImplicitBaseDefaultConstructor(
        std::size_t typeOrdinal,
        std::size_t baseTypeOrdinal,
        ImplicitDefaultConstructorBinding& binding,
        diagnostics::DiagnosticEngine& diagnostics);
    void bindConstructorInitializer(
        std::size_t constructorMemberOrdinal,
        diagnostics::DiagnosticEngine& diagnostics);
    bool resolveConstructorInitializerTarget(
        ConstructorInitializerBinding& binding,
        const ClassMemberSymbol& constructor,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool collectConstructorInitializerArgumentTypes(
        const ConstructorInitializerBinding& binding,
        std::size_t constructorMemberOrdinal,
        std::vector<MethodExpressionTypeBinding>& argumentTypes) const;
    void bindImplicitConstructorInitializer(
        ConstructorInitializerBinding& binding,
        const ClassMemberSymbol& constructor,
        std::span<const MethodExpressionTypeBinding> argumentTypes,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void evaluateConstructorInitializerCandidates(
        ConstructorInitializerBinding& binding,
        std::size_t constructorMemberOrdinal,
        std::span<const MethodExpressionTypeBinding> argumentTypes,
        bool& anyAccessible,
        bool& anyMatchingArity,
        bool& accessibleMatchingArity) const;
    void resolveConstructorInitializerCandidates(
        ConstructorInitializerBinding& binding,
        const ClassMemberSymbol& constructor,
        std::span<const MethodExpressionTypeBinding> argumentTypes,
        bool anyAccessible,
        bool anyMatchingArity,
        bool accessibleMatchingArity,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void selectConstructorInitializerCandidate(
        ConstructorInitializerBinding& binding,
        std::size_t selectedIndex) const;
    void storeConstructorInitializerBinding(
        std::size_t constructorMemberOrdinal,
        ConstructorInitializerBinding&& binding);
    void validateThisConstructorCycles(diagnostics::DiagnosticEngine& diagnostics);
    void bindConstruction(
        const ProjectConstructionSite& site,
        diagnostics::DiagnosticEngine& diagnostics);
    bool initializeConstructionBinding(
        const ProjectConstructionSite& site,
        ObjectConstructionBinding& binding,
        std::optional<std::size_t>& targetOrdinal,
        bool& targetIsStruct,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void initializeConstructionAllocator(
        const ProjectConstructionSite& site,
        ObjectConstructionBinding& binding) const;
    void bindImplicitConstruction(
        ObjectConstructionBinding& binding,
        std::size_t targetOrdinal,
        std::size_t callerMemberOrdinal,
        std::span<const MethodExpressionTypeBinding> argumentTypes,
        bool targetIsStruct,
        const ast::AstNode* expression,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void evaluateConstructionCandidates(
        ObjectConstructionBinding& binding,
        std::size_t callerMemberOrdinal,
        std::span<const MethodExpressionTypeBinding> argumentTypes,
        std::size_t constructorArgumentCount,
        bool& anyAccessible,
        bool& anyMatchingArity,
        bool& accessibleMatchingArity) const;
    bool resolveConstructionAccessibility(
        ObjectConstructionBinding& binding,
        std::span<const MethodExpressionTypeBinding> argumentTypes,
        bool anyAccessible,
        bool anyMatchingArity,
        bool accessibleMatchingArity,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void resolveConstructionCandidates(
        ObjectConstructionBinding& binding,
        std::span<const MethodExpressionTypeBinding> argumentTypes,
        diagnostics::DiagnosticEngine& diagnostics);
    void selectConstructionCandidate(
        ObjectConstructionBinding& binding,
        std::size_t selectedIndex) const;
    ConstructorCandidateBinding evaluateConstructorCandidate(
        const ClassMemberSymbol& constructor,
        std::size_t constructorOrdinal,
        bool accessible,
        std::span<ast::AstNode* const> arguments,
        std::span<const MethodExpressionTypeBinding> argumentTypes,
        const ProjectConversionClassifier& conversions,
        bool& matchingArity) const;
    void preferWholeParameterPackConversion(
        const ClassMemberSymbol& constructor,
        std::span<const MethodExpressionTypeBinding> argumentTypes,
        const ProjectConversionClassifier& conversions,
        ConstructorCandidateBinding& candidate) const;
    void evaluateConstructorArgument(
        const ClassMemberSymbol& constructor,
        std::span<const MethodExpressionTypeBinding> argumentTypes,
        std::size_t argumentOrdinal,
        const ProjectConversionClassifier& conversions,
        ConstructorCandidateBinding& candidate) const;
    std::vector<std::size_t> viableConstructorCandidates(
        const std::vector<ConstructorCandidateBinding>& candidates) const;
    std::vector<std::size_t> nonDominatedConstructorCandidates(
        const std::vector<ConstructorCandidateBinding>& candidates,
        std::span<const std::size_t> viable) const;
    std::optional<MethodExpressionTypeBinding> expressionType(
        const ast::AstNode* syntax,
        std::size_t callerMemberOrdinal) const;
    // NSC-LIMIT-010. Cuando ningun candidato es viable, dice si la unica objecion fue el contrato
    // de propiedad -- y si lo fue, la explica: que argumento, contra que parametro, y cual de los
    // tres incumplimientos. Devuelve true si emitio algo; false deja el diagnostico generico al
    // llamador.
    //
    // Lo comparten el camino de `new` y el de `this(...)`/`base(...)`. Escribirlo dos veces era
    // como se llego a tener la explicacion buena en uno y el generico en el otro.
    //
    // Solo habla con UN candidato de la aridad correcta: con dos o mas, «al argumento 1 le falta
    // move» seria falso para el otro, y nombrar una causa equivocada es peor que ser vago.
    bool explainOwnershipRejection(
        const std::vector<ConstructorCandidateBinding>& candidates,
        std::size_t argumentCount,
        const ast::AstNode* argumentListSyntax,
        const ast::AstNode* fallbackSyntax,
        diagnostics::DiagnosticEngine& diagnostics) const;
    bool constructorAccessible(
        const ClassMemberSymbol& constructor,
        std::size_t callerMemberOrdinal,
        bool baseInitializer) const noexcept;
    bool constructorAccessibleFromType(
        const ClassMemberSymbol& constructor,
        std::size_t callerTypeOrdinal,
        bool baseInitializer) const noexcept;
    bool constructorAccessibleFromProject(
        const ClassMemberSymbol& constructor,
        std::size_t callerProjectOrdinal) const noexcept;
    bool implicitDefaultAccessible(
        std::size_t targetTypeOrdinal,
        std::size_t callerMemberOrdinal,
        bool baseInitializer) const noexcept;
    bool implicitDefaultAccessibleFromType(
        std::size_t targetTypeOrdinal,
        std::size_t callerTypeOrdinal,
        bool baseInitializer) const noexcept;
    bool implicitDefaultAccessibleFromProject(
        std::size_t targetTypeOrdinal,
        std::size_t callerProjectOrdinal) const noexcept;
    bool betterCandidate(
        const ConstructorCandidateBinding& left,
        const ConstructorCandidateBinding& right) const noexcept;

    const ProjectNameLookup* nameLookup_ = nullptr;
    const ProjectMemberModel* memberModel_ = nullptr;
    const ProjectInheritanceModel* inheritanceModel_ = nullptr;
    const ProjectInterfaceModel* interfaceModel_ = nullptr;
    const ProjectMethodBodyModel* methodBodyModel_ = nullptr;
    const ProjectNamespaceFunctionModel* namespaceFunctionModel_ = nullptr;
    std::vector<ObjectConstructionBinding> constructions_;
    std::vector<ConstructorInitializerBinding> constructorInitializerBindings_;
    std::vector<std::optional<std::size_t>> initializerBindingByMember_;
    std::vector<ImplicitDefaultConstructorBinding> implicitDefaultConstructorBindings_;
    std::vector<std::optional<std::size_t>> implicitDefaultBindingByType_;
    ProjectConstructionStatistics statistics_;
    std::span<const PrecompiledSnapshotUnit> precompiledSnapshots_;
    std::unordered_map<std::size_t, ImplicitDefaultConstructorBinding> snapshotImplicitDefaults_;
    std::size_t installedSnapshotClasses_ = 0;
    std::optional<FileId> snapshotCaptureFile_;
    bool built_ = false;
};

const char* constructionResolutionKindName(ConstructionResolutionKind kind) noexcept;
const char* constructorInitializerKindName(ConstructorInitializerKind kind) noexcept;
const char* constructorInitializerResolutionKindName(ConstructorInitializerResolutionKind kind) noexcept;
const char* implicitDefaultConstructorResolutionKindName(
    ImplicitDefaultConstructorResolutionKind kind) noexcept;
}
