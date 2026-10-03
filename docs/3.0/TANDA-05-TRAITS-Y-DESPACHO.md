# Tanda 05 — Traits, interfaces estáticas y despacho / Traits and dispatch

> **Estado 2026-10-03:** `TERMINADA`. `NS3-T05-F01`–`NS3-T05-F07` están terminados:
> declaraciones, defaults, requisitos estáticos, bloques `impl`, coherencia sellada, impls
> condicionales, tipos/constantes asociados, resultados opacos, objetos trait, extensiones
> estáticas, operadores por lang-item estable y transporte NSMETA.
> No se ejecutó baseline.

## Resultado / Outcome

ES: `trait` declara contratos nominales sin storage en edition 3. Admite métodos requeridos/default
y requisitos `static` de propiedad u operador. Constraints concretos se monomorfizan y `dyn Trait`
conserva el despacho por tabla de interfaces. NSMETA 1.28 conserva identidad y orden de slots.

EN: `trait` declares nominal storage-free contracts in edition 3. It supports required/default
methods plus static property/operator requirements. Concrete constraints are monomorphized while
`dyn Trait` preserves interface-table dispatch. Associated types/constants normalize statically.
NSMETA 1.32 also preserves declaration-scoped opaque results, explicit one-word trait objects, and
importable extension-method identity.

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
- `NativeSharp3T05TraitObjectTests` cubre 12 casos de object-safety, ownership, no-boxing,
  upcast, null, DLL y NSMETA 1.31.
- `NativeSharp3T05ExtensionMethodTests` cubre 12 casos de prioridad, imports, generic receivers,
  no-ADL, ambigüedad, ref identity y cruce `.lib/.nsmeta` 1.32.
- `NativeSharp3T05OperatorTraitTests` cubre 20 checks de tabla lang-item, witnesses, promoción,
  consteval, referencias, ambigüedad, short-circuit, inline O0 y cruce `.lib/.nsmeta`; c70 ejecuta
  exit 42 con NSLink.

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

T05 queda funcionalmente cerrado. Su baseline completo continúa reservado para Alberto; este
commit sólo ejecuta baterías dirigidas y regresiones focalizadas.

## Validación / Validation

`nsc` O0; c62 exit 42; MIR static/dynamic; batería F01 14/14; batería F02 11/11; batería F03
16/16; F04 14/14; F05 12/12; F06 12/12; F07 20/20; productor `.lib/.nsmeta` y consumidores con
defaults, impl genérico, asociados, resultados opacos, objetos trait y extensiones estáticas exit
42; c70 operator-traits 6/6 y exit 42.
Sin baseline, CTest general, O1/O2/O3, instalación ni deploy.

Detalle: [trait-declarations.md](language/trait-declarations.md),
[trait-implementations.md](language/trait-implementations.md),
[associated-items.md](language/associated-items.md),
[opaque-result-types.md](language/opaque-result-types.md),
[trait-objects.md](language/trait-objects.md),
[extension-methods.md](language/extension-methods.md),
[NS3-T05-F01-traits.md](items/NS3-T05-F01-traits.md) y
[NS3-T05-F02-impl-coherence.md](items/NS3-T05-F02-impl-coherence.md) y
[NS3-T05-F03-associated-items.md](items/NS3-T05-F03-associated-items.md) y
[NS3-T05-F04-opaque-result-types.md](items/NS3-T05-F04-opaque-result-types.md) y
[NS3-T05-F05-trait-objects.md](items/NS3-T05-F05-trait-objects.md) y
[NS3-T05-F06-extension-methods.md](items/NS3-T05-F06-extension-methods.md) y
[NS3-T05-F07-operator-traits.md](items/NS3-T05-F07-operator-traits.md).
