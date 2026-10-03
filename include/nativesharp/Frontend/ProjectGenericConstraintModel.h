#pragma once

#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
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
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
// NSC-LIMIT-007. QUE INCUMPLIO UNA ESPECIALIZACION, para el diagnostico y para las pruebas.
enum class GenericConstraintViolationKind
{
    // `where T : class` y el argumento es un tipo valor.
    RequiresReferenceType,
    // `where T : Base` y el argumento no deriva de Base.
    DoesNotDeriveFromBase,
    // `where T : IFoo` y el argumento no implementa IFoo.
    DoesNotImplementInterface,
    // El constraint escrito no nombra una clase ni una interfaz -- un enum, un struct, un
    // delegado o un nombre que no resuelve. Es un error de la DECLARACION, no del sitio.
    ConstraintIsNotAClassOrInterface,
    // El mismo tipo nombrado dos veces en una clausula bajo dos escrituras distintas --un alias,
    // un nombre corto y uno calificado--. El parser solo compara texto; esto compara identidad.
    DuplicateConstraint,
    // La clausula infringe la regla de constraint PRIMARIO: `class` junto a una clase base, dos
    // clases base, o una clase base detras de una interfaz.
    InvalidPrimaryConstraint,
    RequiresValueType,
    RequiresDefaultConstructor,
    RequiresCopy,
    RequiresMove,
    RequiresTrackedLifetime,
    RequiresUnmanaged,
    RequiresNotNull,
    EqualityMismatch,
    InvalidConstraintCombination
};

struct GenericConstraintViolation final
{
    GenericConstraintViolationKind kind = GenericConstraintViolationKind::RequiresReferenceType;
    // Ordinal de la ESPECIALIZACION que incumple, o de la PLANTILLA cuando la violacion es de
    // declaracion --`isDeclarationViolation`--, que no tiene sitio de instanciacion al que
    // apuntar porque el error esta en el `where`, no en quien lo usa.
    std::size_t specializationOrdinal = 0;
    std::string parameterName;
    std::string argumentTypeKey;
    std::string constraintTypeKey;
    // Cierto para lo que esta mal ESCRITO en la clausula --un constraint que no nombra clase ni
    // interfaz, o el mismo tipo dos veces bajo dos nombres--. Se comprueba sobre la plantilla y
    // por tanto se reporta aunque nadie la instancie.
    bool isDeclarationViolation = false;
};

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
    bool built_ = false;
};
}
