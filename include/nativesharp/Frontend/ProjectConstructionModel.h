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

// NSC-LIMIT-010. Una transferencia de propiedad elegida por la resolucion de constructores: que
// parametro la recibe y de donde sale. Es la misma informacion que una llamada ordinaria ya
// registraba; el constructor no la tenia porque su resolucion no miraba propiedad en absoluto.
struct ConstructorOwnershipTransfer final
{
    std::size_t parameterOrdinal = 0;
    std::optional<std::size_t> sourceLocalOrdinal;
    std::optional<std::size_t> sourceParameterOrdinal;
};

struct ConstructorCandidateBinding final
{
    std::optional<std::size_t> memberOrdinal;
    bool implicitDefault = false;
    bool accessible = false;
    bool viable = false;
    std::vector<ProjectImplicitConversion> conversions;
    std::vector<std::size_t> argumentParameterOrdinals;
    std::vector<std::size_t> defaultedParameterOrdinals;
    std::size_t defaultedParameterCount = 0;
    // NSC-LIMIT-010. Viabilidad de PROPIEDAD por argumento, en paralelo a `conversions`, que solo
    // lleva la fisica. Un candidato puede ser fisicamente valido y aun asi incumplir el contrato:
    // `new Caja(duenio)` sin `move` convierte perfectamente y sigue siendo ilegal.
    //
    // Se guarda el veredicto de CADA argumento y no un booleano por candidato porque el
    // diagnostico tiene que nombrar el argumento concreto y decir cual de los tres
    // incumplimientos fue.
    std::vector<OwnershipArgumentFit> ownershipFits;
    // Las transferencias que este candidato exigiria si saliera elegido.
    std::vector<ConstructorOwnershipTransfer> ownershipTransfers;
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
    std::vector<std::size_t> selectedArgumentParameterOrdinals;
    std::vector<const ast::AstNode*> selectedDefaultArguments;
    std::vector<std::size_t> selectedDefaultArgumentParameterOrdinals;
    // NSC-LIMIT-010: `this(...)` y `base(...)` transfieren propiedad con las mismas reglas
    // que un `new`, asi que registran las mismas transferencias.
    std::vector<ConstructorOwnershipTransfer> selectedOwnershipTransfers;
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
    std::vector<std::size_t> selectedArgumentParameterOrdinals;
    std::vector<const ast::AstNode*> selectedDefaultArguments;
    std::vector<std::size_t> selectedDefaultArgumentParameterOrdinals;
    // NSC-LIMIT-010. Las transferencias del candidato ELEGIDO, copiadas aqui para que quien
    // lea el binding final no tenga que volver a recorrer los candidatos evaluados.
    std::vector<ConstructorOwnershipTransfer> selectedOwnershipTransfers;
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
