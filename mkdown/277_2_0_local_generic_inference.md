# 277 — Inferencia genérica local / Local generic inference

## Resumen / Summary

NativeSharp permite omitir argumentos de tipo en llamadas a métodos genéricos cuando los tipos
estáticos de los argumentos locales determinan una sustitución completa y única. Las llamadas
inferidas reutilizan la monomorfización, cache e identidad física de las llamadas explícitas.

NativeSharp allows generic method type arguments to be omitted when local argument static types
determine one complete, unique substitution. Inferred calls reuse the same monomorphization,
cache, and physical identity as explicit calls.

## Archivos y funciones / Files and functions

- `include/nativesharp/Frontend/ProjectGenericModel.h` — amplía `GenericLookupScope` con tipos de
  valores visibles y declara inferencia de llamadas, expresiones y unificación estructural.
- `src/Frontend/ProjectGenericModel.cpp` — `inferGenericCall()` selecciona templates visibles y
  construye los argumentos; `inferExpressionType()` obtiene evidencia local; `inferTypeArguments()`
  unifica parámetros directos, genéricos anidados, pointers y arrays.
- `tests/unit/GenericInferenceTests.cpp` — cubre literales, locals, `var`, casts, arrays, nesting,
  argumentos nombrados/opcionales, `ref`, llamadas anidadas, especificidad y rechazos.
- `CMakeLists.txt` — registra `nsc.language.generic-inference`.
- `docs/1.0.1/` — actualiza changelog, estado, roadmap, especificación, libro y conformidad.

## Implementación y decisiones / Implementation and decisions

La inferencia ocurre en `ProjectGenericModel`, justo antes de `processGenericCall()`. No existe una
segunda monomorfización: la pasada agrega al nodo la misma lista de `TypeSyntax` que habría producido
`F<int>(...)`, y el camino existente crea o reutiliza la especialización canónica.

Inference runs in `ProjectGenericModel`, immediately before `processGenericCall()`. There is no
second monomorphizer: the pass adds the same `TypeSyntax` list that `F<int>(...)` would have
produced, then the existing path creates or reuses the canonical specialization.

El scope conserva snapshots de tipos escritos en parámetros, campos y locals anteriores. Es
importante copiar antes de `processTypeSyntax()`: esa función reemplaza `Box<int>` por su nombre
físico monomorfizado, que ya no conserva la estructura necesaria para deducir el `T` de `Box<T>`.
Los locals `var` capturan el tipo del inicializador antes de esa reescritura.

The scope keeps snapshots of types written on parameters, fields, and preceding locals. Capturing
before `processTypeSyntax()` matters because that function replaces `Box<int>` with its physical
monomorphized name, which no longer retains the structure needed to infer `T` from `Box<T>`. `var`
locals snapshot their initializer type before that rewrite.

La unificación trata los modificadores del parámetro como un sufijo de la forma real. Así `T[]`
contra `int[]` quita el array e infiere `int`; `T*` contra `int**` infiere `int*`. Los contenedores
nominales deben coincidir y sus argumentos se unifican recursivamente. Una repetición de `T` debe
producir el mismo spelling exacto.

Unification treats parameter modifiers as a suffix of the actual shape. Therefore `T[]` against
`int[]` removes the array and infers `int`, while `T*` against `int**` infers `int*`. Nominal
containers must match and their arguments unify recursively. Repeated `T` occurrences must produce
the same exact spelling.

La inferencia procesa primero una llamada genérica usada como argumento de otra. Los overloads se
ordenan por información estructural: `Box<T>` o `T[]` es más específico que `T`. Si dos candidatos
igualmente específicos exigen sustituciones incompatibles, se pide `<...>` explícito en lugar de
elegir por orden de declaración. Si hay un overload no genérico de igual nombre y aridad, la pasada
temprana deja la llamada al resolvedor ordinario; el genérico puede solicitarse explícitamente.

Nested inferred calls are processed before their containing call. Generic overloads rank by
structural information: `Box<T>` or `T[]` is more specific than bare `T`. Equally specific,
incompatible substitutions require explicit `<...>` instead of declaration-order guessing. A
same-name/same-arity non-generic overload remains an ordinary resolution case; the generic can be
requested explicitly.

## Evidencia dirigida / Focused evidence

- `Identity(local)`, sufijos literales y casts deducen tipos escalares exactos.
- `First(values)` deduce el elemento de `T[]`; `Unbox(box)` deduce el argumento de `Box<T>`.
- Un local `var` conserva la forma genérica de su inicializador para llamadas posteriores.
- Argumentos nombrados se mapean por nombre; parámetros opcionales omitidos no impiden inferir.
- `ref T` se unifica con la dirección del argumento y llamadas inferidas anidadas propagan retorno.
- Una forma `Box<T>`/`T[]` vence a `T` por especificidad estructural.
- `T` sólo en retorno, `null` y apariciones repetidas incompatibles exigen argumentos explícitos.
- La suite enfocada pasa 13 checks y el programa O0 dirigido compila y ejecuta con código 0.

No se ejecutó baseline para esta entrega. / No baseline was run for this delivery.

## Commit de referencia / Reference commit

**Título / Title**

`Añade inferencia genérica local / Add local generic inference`

**Descripción / Description**

`Deduce argumentos genéricos desde tipos locales y formas anidadas antes de monomorfizar, reutilizando la identidad canónica y exigiendo sintaxis explícita cuando falta evidencia. / Infers generic arguments from local types and nested shapes before monomorphization, reusing canonical identity and requiring explicit syntax when evidence is incomplete.`
