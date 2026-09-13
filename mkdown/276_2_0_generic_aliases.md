# 276 — Aliases genéricos / Generic aliases

## Resumen / Summary

NativeSharp permite parametrizar aliases de tipo con `using Nombre<T> = Tipo;`. El compilador
expande cada uso antes de registrar templates o calcular especializaciones. El alias no crea tipo,
layout, descriptor, metadata ni símbolo ABI propio.

NativeSharp allows parameterized type aliases with `using Name<T> = Type;`. The compiler expands
every use before template registration or specialization. An alias creates no type, layout,
descriptor, metadata entry, or ABI symbol of its own.

## Archivos y funciones / Files and functions

- `src/Parser/Parser.cpp` — `parseUsingDirective()` reconoce parámetros genéricos en un alias,
  reutiliza la gramática común de listas de parámetros y conserva la distinción con aliases de
  namespace.
- `include/nativesharp/Frontend/ProjectGenericModel.h` — amplía `GenericLookupScope` con aliases de
  tipo y parámetros que los ocultan; declara la pasada de expansión temprana.
- `src/Frontend/ProjectGenericModel.cpp` — `expandAliasesInNode()`, `expandTypeAliases()` y
  `cloneAliasType()` normalizan targets, sustituyen argumentos, preservan modificadores y rechazan
  aridad, ciclos y ownership contradictorio antes de la monomorfización.
- `tests/unit/GenericAliasTests.cpp` — cubre cadenas, nesting, repetición y reordenamiento de
  argumentos, punteros, arreglos, shadowing, aridad y ciclos.
- `CMakeLists.txt` — registra la suite enfocada `nsc.language.generic-aliases`.
- `docs/1.0.1/` — actualiza estado, changelog, roadmap, especificación, libro y mapas de conformidad.

## Implementación y decisiones / Implementation and decisions

La expansión vive al inicio de `ProjectGenericModel::build()`. Name lookup era demasiado tarde:
para entonces `Alias<int>` ya podría haberse interpretado como una plantilla distinta o un target
abierto `Box<T>` podría haberse materializado con un parámetro libre. Después de la pasada temprana
el resto del compilador sólo observa el tipo subyacente.

Expansion runs at the start of `ProjectGenericModel::build()`. Name lookup was too late: by then
`Alias<int>` could already have been interpreted as a separate template, or an open `Box<T>` target
could have been materialized with a free parameter. After the early pass, the rest of the compiler
sees only the underlying type.

Los targets se normalizan en su scope de declaración antes de expandir usos. Así, una cadena de
aliases conserva lexical scoping aunque un scope interior declare otro alias con el mismo nombre.
Los parámetros del alias se agregan al conjunto de nombres que ocultan aliases exteriores.

Targets are normalized in their declaration scope before uses are expanded. This preserves lexical
scoping even when an inner scope declares a same-named alias. Alias parameters join the set of names
that shadow outer aliases.

La sustitución clona únicamente sintaxis de tipo. No reutiliza el clonador de especializaciones,
porque ese camino adjunta procedencia del proyecto de instanciación y convertiría una reescritura
lexical en una identidad cross-project falsa. Los bounds de arreglos son expresiones inmutables del
mismo arena y se comparten; los argumentos y el contenedor `TypeSyntax` sí se clonan.

Substitution clones type syntax only. It deliberately avoids the specialization cloner, whose job
is to attach instantiation-project provenance and would turn a lexical rewrite into a false
cross-project identity. Immutable array-bound expressions remain in the same arena; type arguments
and their containing `TypeSyntax` nodes are cloned.

Los modificadores del target quedan debajo de los escritos en el uso. Por ello
`using Pointer<T> = T*; Pointer<int>[]` conserva `int*[]`. Un qualifier de ownership escrito en el
uso puede completar un target no calificado, pero dos qualifiers contradictorios se rechazan.

Target modifiers remain inside use-site modifiers, so `Pointer<int>[]` preserves `int*[]`. A
use-site ownership qualifier may complete an unqualified target, while contradictory qualifiers are
rejected.

## Evidencia dirigida / Focused evidence

- `BoxOf<int>` y una cadena `Chain<int>` reutilizan una sola especialización `Box<int>`.
- `Twice<long>` produce `Pair<long, long>` y `Flip<int, long>` produce `Pair<long, int>`.
- `Nested<int>` expande argumentos genéricos anidados.
- `Pointer<int>` y `Quad<int>` llegan a method bodies como puntero y arreglo fijo ordinarios.
- Un parámetro `T` oculta un alias exterior también llamado `T`.
- Aridad insuficiente, aridad excesiva y ciclos indirectos producen diagnósticos propios.
- Las suites enfocadas de aliases, Stage 44 y name lookup pasan: 15, 81 y 78 checks.
- Un productor O0 publicó `BoxOf<int>` únicamente como su `Box<int>` expandido; `.lib + .nsmeta`
  pasaron el verificador, el consumidor compiló sólo con `LibraryReference` y ejecutó con código 0.

No se ejecutó baseline para esta entrega. / No baseline was run for this delivery.

## Commit de referencia / Reference commit

**Título / Title**

`Añade aliases genéricos / Add generic aliases`

**Descripción / Description**

`Permite parametrizar aliases de tipo, los expande antes de especializar genéricos y conserva identidad, modificadores y ownership del tipo subyacente. / Allows parameterized type aliases, expands them before generic specialization, and preserves the underlying type's identity, modifiers, and ownership.`
