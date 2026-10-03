# Tanda 05 — Traits, interfaces estáticas y despacho / Traits and dispatch

> **Estado 2026-10-03:** `IMPLEMENTING`. `NS3-T05-F01` está terminado: declaraciones, métodos
> requeridos/default, propiedades y operadores estáticos requeridos, conflictos y NSMETA 1.28.
> F02–F07 permanecen pendientes. No se ejecutó baseline.

## Resultado / Outcome

ES: `trait` declara contratos nominales sin storage en edition 3. Admite métodos requeridos/default
y requisitos `static` de propiedad u operador. Constraints concretos se monomorfizan y `dyn Trait`
conserva el despacho por tabla de interfaces. NSMETA 1.28 conserva identidad y orden de slots.

EN: `trait` declares nominal storage-free contracts in edition 3. It supports required/default
methods plus static property/operator requirements. Concrete constraints are monomorphized while
`dyn Trait` preserves interface-table dispatch. NSMETA 1.28 preserves identity and slot order.

## Archivos y cambios / Files and changes

- `Parser`/`TypeSyntax` aceptan y conservan `trait`/`dyn` sólo en edition 3.
- `ProjectNameLookup` rechaza destinos `dyn` que no sean interface/trait.
- `ProjectGenericModel` conserva `dyn` al expandir aliases y monomorfizar.
- `c62_ns3_traits_dispatch` prueba static/dynamic dispatch hasta PE O0.
- `ProjectInterfaceModel` resuelve defaults, conflictos y requisitos estáticos sin añadir layout.
- `LibraryMetadata` 1.28 publica el marcador trait y el orden normativo de métodos.
- `NativeSharp3T05TraitDeclarationTests` cubre 14 casos, incluido productor/consumidor separado.

## Mejora / Improvement

ES: Un trait no altera layout ni crea un runtime paralelo. La implementación en la lista base evita
impls huérfanos por construcción; el constraint especializado hace llamada directa y `dyn` hace
`interface-instance-call`. Defaults y requisitos cruzan `.nsmeta` sin reordenar slots.

EN: Traits add no layout or parallel runtime. Base-list implementation prevents orphan impls by
construction; specialized constraints use direct calls and `dyn` uses `interface-instance-call`.
Defaults and requirements cross `.nsmeta` without slot reordering.

## Límite / Boundary

F01 no equivale al gate T05. Bloques `impl`/coherencia, associated items, opaque results,
object-safety completa, extension lookup y operadores resueltos mediante traits siguen abiertos en
F02–F07.

## Validación / Validation

`nsc` O0; c62 exit 42; MIR static/dynamic; batería F01 14/14; productor `.lib/.nsmeta` y
consumidor con default exit 42. Sin baseline, CTest general, O1/O2/O3, instalación ni deploy.

Detalle: [trait-declarations.md](language/trait-declarations.md) y
[NS3-T05-F01-traits.md](items/NS3-T05-F01-traits.md).
