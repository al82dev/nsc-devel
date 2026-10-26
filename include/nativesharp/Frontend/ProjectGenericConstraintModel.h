#pragma once

#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Frontend/GenericConstraintViolation.h"
#include "nativesharp/Frontend/GenericConstraintViolationKind.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
namespace internal
{
struct PendingDiagnostic;
}

// LA FASE QUE VALIDA LOS `where`, y por que es una fase aparte.
//
// Las clausulas se leen en el parser y se guardan sin resolver, porque cuando el parser corre no
// existe ningun modelo de tipos. La materializacion generica --que crea las especializaciones-- es
// la SEGUNDA fase de toda la cadena: syntax -> generics -> names -> members -> inheritance ->
// interfaces -> methodBodies. Cuando materializa, la herencia y las interfaces todavia no existen,
// asi que tampoco puede validar alli.
//
// De modo que la validacion tiene un unico sitio posible: despues de construir interfaces y antes
// de analizar los cuerpos. Antes no hay con que responder «este tipo implementa esa interfaz»;
// despues, los cuerpos ya se habrian analizado sobre una especializacion invalida y el error
// saldria de dentro del cuerpo sustituido -- que es el muro de errores de plantillas que este ID
// existe para quitar.
class ProjectGenericConstraintModel final
{
public:
    bool build(
        const ProjectGenericModel& genericModel,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectInterfaceModel& interfaceModel,
        diagnostics::DiagnosticEngine& diagnostics);

    std::span<const GenericConstraintViolation> violations() const noexcept;
    bool isBuilt() const noexcept;
    std::string_view normalizedConstraintIdentity(std::size_t templateOrdinal) const noexcept;
    std::span<const std::string> normalizedConstraintAtoms(
        std::size_t templateOrdinal) const noexcept;

    // LO QUE UNA ESPECIALIZACION DE UN GRUPO DE SOBRECARGAS NO PUEDE DECIDIR SOLA.
    //
    // Cuando varias plantillas comparten nombre y aridad generica, la fase generica las
    // materializa todas: elegir entre ellas necesita los TIPOS de los argumentos, y esa fase no
    // los tiene. Intentar decidirlo aqui --contando parametros-- separaba `F<T>(T x)` de
    // `F<T>(T x, int)` y no separaba `F<T>(T x)` de `F<T>(T[] x)`, que es el caso normal.
    //
    // Asi que este modelo NO decide: guarda las violaciones de las especializaciones agrupadas
    // sin emitirlas, y la resolucion ordinaria de sobrecargas --que si conoce los tipos, las
    // conversiones y la propiedad-- pregunta por la candidata que de verdad esta considerando.
    // Una violacion solo se convierte en error cuando ninguna candidata fisicamente viable la
    // esquiva.
    //
    // Las especializaciones que NO forman grupo emiten su error al construir, como siempre: no
    // hay nada que esperar. Lo que NO cambia con el grupo es la respuesta de esta funcion: una
    // especializacion que incumple es INVALIDA se aplace su diagnostico o no, y por tanto su
    // cuerpo no se analiza ni se baja. Diferir el DIAGNOSTICO y emitir el CODIGO son dos
    // decisiones distintas, y colapsarlas dejaba pasar un segundo error --sobre un cuerpo
    // sustituido con un `T` que la clausula prohibe-- encima del error de verdad.
    //
    // Acepta indistintamente el nodo de un MIEMBRO especializado y el del TIPO que lo declara:
    // un metodo perfectamente valido de una `Caja<int>` invalida tampoco se analiza, porque su
    // `T` es el mismo que la clausula rechazo.
    //
    // `nullptr` responde `true`: lo que no salio de una especializacion no tiene clausula que
    // incumplir.
    bool specializationSatisfiesConstraints(const ast::AstNode* specializedSyntax) const noexcept;
    // `callRange` es el sitio de ESTA llamada. La especializacion se cachea por clave, de modo
    // que guarda el rango de la PRIMERA instanciacion y lo conserva: reportar ese rango cuando el
    // programa falla en la tercera llamada manda al lector a una linea correcta. El sitio lo sabe
    // quien resuelve la llamada, y por eso viaja desde alli.
    void reportSpecializationConstraintViolations(
        const ast::AstNode* specializedSyntax,
        SourceRange callRange,
        diagnostics::DiagnosticEngine& diagnostics) const;

private:
    // Un diagnostico redactado y no emitido. Vive hasta que la resolucion de sobrecargas diga si
    // la candidata que lo lleva es la que el programa usa.
    struct DeferredDiagnostic final
    {
        diagnostics::DiagnosticId id;
        SourceRange range;
        std::string message;
        GenericConstraintViolation violation;
    };

    // Lo que esta mal ESCRITO en un `where`, comprobado sobre la PLANTILLA y por tanto reportado
    // aunque nadie la instancie: un constraint que no nombra clase ni interfaz, y el mismo tipo
    // nombrado dos veces bajo dos escrituras distintas.
    void validateTemplateClauses(
        const ProjectGenericModel& genericModel,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        diagnostics::DiagnosticEngine& diagnostics);
    static std::size_t constraintParameterOrdinal(
        const GenericTemplateRecord& templateRecord,
        const ast::GenericConstraintClause& clause);
    static void appendTemplateClauseAtoms(
        const ast::GenericConstraintClause& clause,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        std::vector<std::string>& clauseAtoms);
    bool validateTemplateTypeConstraint(
        std::size_t templateOrdinal,
        const ast::GenericConstraintClause& clause,
        const ast::TypeSyntax* constraintSyntax,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        std::unordered_map<std::string, std::string>& seenByKey,
        std::vector<std::pair<const ast::TypeSyntax*, std::size_t>>& baseClasses,
        std::size_t& acceptedConstraints,
        std::vector<std::string>& clauseAtoms,
        diagnostics::DiagnosticEngine& diagnostics);
    void normalizeTemplateClause(
        std::size_t templateOrdinal,
        std::size_t parameterOrdinal,
        const ast::GenericConstraintClause& clause,
        const std::vector<std::string>& clauseAtoms,
        std::vector<std::string>& templateAtoms,
        diagnostics::DiagnosticEngine& diagnostics);
    void validatePrimaryTemplateConstraint(
        std::size_t templateOrdinal,
        const ast::GenericConstraintClause& clause,
        const std::vector<std::pair<const ast::TypeSyntax*, std::size_t>>& baseClasses,
        diagnostics::DiagnosticEngine& diagnostics);
    void validateTemplateClause(
        std::size_t templateOrdinal,
        const GenericTemplateRecord& templateRecord,
        const ast::GenericConstraintClause& clause,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        std::vector<std::string>& templateAtoms,
        diagnostics::DiagnosticEngine& diagnostics);
    void finalizeTemplateConstraintIdentity(
        std::size_t templateOrdinal,
        const std::vector<std::string>& templateAtoms);
    void validateControlledSpecializations(
        const ProjectGenericModel& genericModel,
        diagnostics::DiagnosticEngine& diagnostics);
    void reportControlledSpecializationViolation(
        std::span<const GenericTemplateRecord> templates,
        std::size_t ordinal,
        std::string message,
        diagnostics::DiagnosticEngine& diagnostics);
    void notePreviousControlledSpecialization(
        std::span<const GenericTemplateRecord> templates,
        std::size_t ordinal,
        diagnostics::DiagnosticEngine& diagnostics) const;
    void validateControlledSpecializationPair(
        std::span<const GenericTemplateRecord> templates,
        std::size_t leftOrdinal,
        std::size_t rightOrdinal,
        std::unordered_set<std::size_t>& hasFallback,
        diagnostics::DiagnosticEngine& diagnostics);
    void validateControlledSpecializationFamily(
        std::span<const GenericTemplateRecord> templates,
        const std::vector<std::size_t>& ordinals,
        diagnostics::DiagnosticEngine& diagnostics);
    void initializeBuildState(std::size_t templateCount);
    void collectInvalidTemplateOrdinals();
    static void appendCapabilityViolation(
        bool required,
        bool satisfied,
        GenericConstraintViolationKind kind,
        std::string_view spelling,
        std::string_view expectation,
        const GenericSpecializationRecord& specialization,
        const GenericTemplateRecord& templateRecord,
        const ast::GenericConstraintClause& clause,
        const std::string& argumentKey,
        const std::string& argumentName,
        const std::string& templateModuleSuffix,
        std::vector<internal::PendingDiagnostic>& pending);
    static void appendCapabilityViolations(
        const GenericSpecializationRecord& specialization,
        const GenericTemplateRecord& templateRecord,
        const ast::GenericConstraintClause& clause,
        const ast::TypeSyntax* argumentSyntax,
        const NamedTypeDeclaration* argument,
        std::size_t argumentOrdinal,
        const std::string& argumentKey,
        const std::string& argumentName,
        const std::string& templateModuleSuffix,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        std::vector<internal::PendingDiagnostic>& pending);
    static void appendEqualityViolation(
        const GenericSpecializationRecord& specialization,
        const GenericTemplateRecord& templateRecord,
        const ast::GenericConstraintClause& clause,
        const ast::TypeSyntax* argumentSyntax,
        const std::string& argumentName,
        const std::string& templateModuleSuffix,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        std::vector<internal::PendingDiagnostic>& pending);
    static void appendReferenceTypeViolation(
        const GenericSpecializationRecord& specialization,
        const GenericTemplateRecord& templateRecord,
        const ast::GenericConstraintClause& clause,
        const ast::TypeSyntax* argumentSyntax,
        const NamedTypeDeclaration* argument,
        const std::string& argumentKey,
        const std::string& argumentName,
        const std::string& templateModuleSuffix,
        std::vector<internal::PendingDiagnostic>& pending);
    static bool satisfiesThreadSafetyConstraint(
        const NamedTypeDeclaration& argument,
        std::size_t argumentOrdinal,
        bool sendMarker,
        const ProjectMemberModel& memberModel);
    static bool satisfiesTypeConstraint(
        const NamedTypeDeclaration& argument,
        std::size_t argumentOrdinal,
        const NamedTypeDeclaration& constraint,
        std::size_t constraintOrdinal,
        const ProjectMemberModel& memberModel,
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectInterfaceModel& interfaceModel);
    static void appendTypeConstraintViolation(
        const GenericSpecializationRecord& specialization,
        const GenericTemplateRecord& templateRecord,
        const ast::GenericConstraintClause& clause,
        const ast::TypeSyntax* constraintSyntax,
        const NamedTypeDeclaration* argument,
        std::size_t argumentOrdinal,
        const std::string& argumentKey,
        const std::string& argumentName,
        const std::string& templateModuleSuffix,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectInterfaceModel& interfaceModel,
        std::vector<internal::PendingDiagnostic>& pending);
    static void appendTypeConstraintViolations(
        const GenericSpecializationRecord& specialization,
        const GenericTemplateRecord& templateRecord,
        const ast::GenericConstraintClause& clause,
        const NamedTypeDeclaration* argument,
        std::size_t argumentOrdinal,
        const std::string& argumentKey,
        const std::string& argumentName,
        const std::string& templateModuleSuffix,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectInterfaceModel& interfaceModel,
        std::vector<internal::PendingDiagnostic>& pending);
    static void validateSpecializationClause(
        const GenericSpecializationRecord& specialization,
        const GenericTemplateRecord& templateRecord,
        const ast::GenericConstraintClause& clause,
        const std::string& templateModuleSuffix,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectInterfaceModel& interfaceModel,
        std::vector<internal::PendingDiagnostic>& pending);
    void storeSpecializationViolations(
        const GenericSpecializationRecord& specialization,
        const std::vector<internal::PendingDiagnostic>& pending,
        std::vector<internal::PendingDiagnostic>& ungrouped);
    void validateSpecializationMemberClauses(
        const GenericSpecializationRecord& specialization,
        const GenericTemplateRecord& templateRecord,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectInterfaceModel& interfaceModel);
    void validateSpecialization(
        const GenericSpecializationRecord& specialization,
        std::span<const GenericTemplateRecord> templates,
        const ProjectNameLookup& nameLookup,
        const ProjectMemberModel& memberModel,
        const ProjectInheritanceModel& inheritanceModel,
        const ProjectInterfaceModel& interfaceModel,
        std::vector<internal::PendingDiagnostic>& ungrouped);
    void emitUngroupedViolations(
        const std::vector<internal::PendingDiagnostic>& ungrouped,
        diagnostics::DiagnosticEngine& diagnostics);
    // Las tres formas de infringir la regla de constraint primario comparten registro y
    // diagnostico; solo cambia el mensaje, que es lo unico que las distingue para el lector.
    void reportPrimaryConstraint(
        diagnostics::DiagnosticEngine& diagnostics,
        std::size_t templateOrdinal,
        const ast::GenericConstraintClause& clause,
        const ast::TypeSyntax* offending,
        const std::string& message);

    std::vector<GenericConstraintViolation> violations_;
    // specializedSyntax -> lo que esa candidata incumple, para poder EXPLICARLO mas tarde. Solo
    // de especializaciones agrupadas: son las unicas cuyo diagnostico se aplaza.
    std::unordered_map<const ast::AstNode*, std::vector<DeferredDiagnostic>> deferredBySpecialization_;
    // Y TODAS las especializaciones invalidas, se aplace su diagnostico o no.
    //
    // Son dos preguntas distintas y estaban colapsadas en un mapa: «hay que diferir este
    // diagnostico» y «esta especializacion es invalida». Una especializacion SIN grupo emite su
    // error de inmediato y no entra en el mapa de diferidos, asi que
    // `specializationSatisfiesConstraints` respondia `true` sobre ella y su cuerpo --sustituido
    // con un `T` que la clausula prohibe-- se analizaba igual, anadiendo un segundo error sobre
    // codigo que el programa no usa.
    std::unordered_set<const ast::AstNode*> invalidSpecializations_;
    // Y LA TERCERA FORMA DE INVALIDEZ, que no nace de ningun argumento: la PLANTILLA esta mal
    // declarada. `where T : int` es un error de la clausula --`int` no nombra clase ni interfaz--
    // y lo diagnostica la primera pasada, sobre la declaracion, se instancie o no.
    //
    // Lo que fallaba: la segunda pasada SALTA ese constraint --no es clase ni interfaz, no hay
    // nada que comprobar contra el argumento--, de modo que `pending` queda vacio y la
    // especializacion nunca entraba en `invalidSpecializations_`. Su cuerpo se analizaba, y el
    // usuario recibia dos errores: el `where` mal escrito, que es el suyo, y `x.MetodoInexistente`
    // salido de un cuerpo que nunca debio mirarse.
    //
    // Se llena desde `violations_`, no desde cada sitio que reporta: toda violacion de
    // DECLARACION lleva ya `isDeclarationViolation` y el ordinal de la PLANTILLA en
    // `specializationOrdinal`. Recolectarlo de ahi hace que una regla de declaracion que se
    // anada manana entre sola, sin que nadie se acuerde de tocar este conjunto.
    std::unordered_set<std::size_t> invalidTemplateOrdinals_;
    // Parameter ordinals plus sorted semantic atoms. This is stable under source reordering and
    // is the identity consumed by diagnostics/metadata/cache-facing tests.
    std::vector<std::string> normalizedConstraintIdentities_;
    std::vector<std::vector<std::string>> normalizedConstraintAtoms_;
    bool built_ = false;
};
}
