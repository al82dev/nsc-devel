#pragma once

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
    // NS304-T11-F05: `where T : ref-like` and the argument is not a stack-only borrowed view.
    RequiresRefLike,
    EqualityMismatch,
    InvalidConstraintCombination
};
}
