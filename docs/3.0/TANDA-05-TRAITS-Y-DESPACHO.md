# Tanda 05 — Traits, interfaces estáticas y despacho / Traits and dispatch

> **Estado 2026-10-03:** `IMPLEMENTING`. `NS3-T05-F01`–`NS3-T05-F04` están terminados:
> declaraciones, defaults, requisitos estáticos, bloques `impl`, coherencia sellada, impls
> condicionales, tipos/constantes asociados, resultados opacos y transporte NSMETA. F05–F07
> permanecen pendientes.
> No se ejecutó baseline.

## Resultado / Outcome

ES: `trait` declara contratos nominales sin storage en edition 3. Admite métodos requeridos/default
y requisitos `static` de propiedad u operador. Constraints concretos se monomorfizan y `dyn Trait`
conserva el despacho por tabla de interfaces. NSMETA 1.28 conserva identidad y orden de slots.

EN: `trait` declares nominal storage-free contracts in edition 3. It supports required/default
methods plus static property/operator requirements. Concrete constraints are monomorphized while
`dyn Trait` preserves interface-table dispatch. Associated types/constants normalize statically.
NSMETA 1.30 also preserves declaration-scoped opaque result identity and in-place layout.

## Archivos y cambios / Files and changes

- `Parser`/`TypeSyntax` aceptan y conservan `trait`/`dyn` sólo en edition 3.
- `ProjectNameLookup` rechaza destinos `dyn` que no sean interface/trait.
- `ProjectGenericModel` conserva `dyn` al expandir aliases y monomorfizar.
- `c62_ns3_traits_dispatch` prueba static/dynamic dispatch hasta PE O0.
- `ProjectInterfaceModel` resuelve defaults, conflictos y requisitos estáticos sin añadir layout.
- `LibraryMetadata` 1.28 publica el marcador trait y el orden normativo de métodos.
- `NativeSharp3T05TraitDeclarationTests` cubre 14 casos, incluido productor/consumidor separado.
- `NativeSharp3T05ImplCoherenceTests` cubre ownership, duplicados, privacidad, condicionales y
  round-trip genérico productor/consumidor.
- `NativeSharp3T05AssociatedItemTests` cubre 16 casos de proyección, defaults, igualdad, ciclos,
  constantes, genéricos y productor/consumidor NSMETA 1.29.
- `NativeSharp3T05OpaqueResultTests` cubre 14 casos de identidad, representación única, herencia,
  asociados, inferencia genérica, módulos separados y NSMETA 1.30.

## Mejora / Improvement

ES: Un trait no altera layout ni crea un runtime paralelo. Un bloque `impl` sólo puede extender una
clase propiedad del módulo; el constraint especializado hace llamada directa y `dyn` hace
`interface-instance-call`. Tipos y constantes asociados se normalizan antes del backend. Defaults,
requisitos e impls genéricos cruzan `.nsmeta` sin reordenar slots ni delegar coherence al runtime.

EN: Traits add no layout or parallel runtime. Sealed coherence lets an impl extend only a class
owned by its module; specialized constraints use direct calls and `dyn` uses
`interface-instance-call`. Defaults, requirements, and generic impls cross `.nsmeta` without slot
reordering or runtime coherence.

## Límite / Boundary

F01–F04 no equivalen al gate T05. Object-safety completa, extension lookup y operadores resueltos
mediante traits siguen abiertos en F05–F07.

## Validación / Validation

`nsc` O0; c62 exit 42; MIR static/dynamic; batería F01 14/14; batería F02 11/11; batería F03
16/16; F04 14/14; productor `.lib/.nsmeta` y consumidores con defaults, impl genérico, asociados y
resultados opacos exit 42.
Sin baseline, CTest general, O1/O2/O3, instalación ni deploy.

Detalle: [trait-declarations.md](language/trait-declarations.md),
[trait-implementations.md](language/trait-implementations.md),
[associated-items.md](language/associated-items.md),
[opaque-result-types.md](language/opaque-result-types.md),
[NS3-T05-F01-traits.md](items/NS3-T05-F01-traits.md) y
[NS3-T05-F02-impl-coherence.md](items/NS3-T05-F02-impl-coherence.md) y
[NS3-T05-F03-associated-items.md](items/NS3-T05-F03-associated-items.md) y
[NS3-T05-F04-opaque-result-types.md](items/NS3-T05-F04-opaque-result-types.md).
