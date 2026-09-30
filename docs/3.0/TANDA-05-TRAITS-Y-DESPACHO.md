# Tanda 05 — Traits, interfaces estáticas y despacho / Traits and dispatch

> **Estado 2026-09-29:** `IMPLEMENTING`. La tanda operativa 12 conecta una vertical productiva de
> `trait`, constraints con despacho estático y `dyn Trait` sobre el ABI existente de interfaces,
> incluido consumo `.lib/.nsmeta`. Associated items, `some`, impls ampliados, extensiones,
> operadores, ABI/tooling completos y el gate DLL siguen abiertos. Véase
> [TANDA-00-AUDITORIA-REAL-T00-T17.md](TANDA-00-AUDITORIA-REAL-T00-T17.md).

## Resultado / Outcome

ES: `trait` ya declara contratos nominales sin storage en edition 3. Constraints concretos se
monomorfizan y `dyn Trait` conserva el despacho por tabla de interfaces de forma explícita en
fuente/MIR. EN: `trait` now declares nominal storage-free contracts in edition 3. Concrete
constraints are monomorphized while `dyn Trait` preserves interface-table dispatch explicitly in
source and MIR.

## Archivos y cambios / Files and changes

- `Parser`/`TypeSyntax` aceptan y conservan `trait`/`dyn` sólo en edition 3.
- `ProjectNameLookup` rechaza destinos `dyn` que no sean interface/trait.
- `ProjectGenericModel` conserva `dyn` al expandir aliases y monomorfizar.
- `c62_ns3_traits_dispatch` prueba static/dynamic dispatch hasta PE O0.

## Mejora / Improvement

ES: Un trait no altera layout ni crea un runtime paralelo. La implementación en la lista base evita
impls huérfanos por construcción; el constraint especializado hace llamada directa y `dyn` hace
`interface-instance-call`. El mismo contrato cruza `.nsmeta`.

EN: Traits add no layout or parallel runtime. Base-list implementation prevents orphan impls by
construction; specialized constraints use direct calls and `dyn` uses `interface-instance-call`.
The same contract crosses `.nsmeta`.

## Límite / Boundary

La vertical productiva no equivale al gate T05. Associated items, opaque results, conditional impls,
extension lookup, operadores por trait, un posible ABI dyn alternativo y tooling siguen abiertos.

## Validación / Validation

`nsc` Debug/O0; c62 exit 42; MIR static/dynamic; productor `.lib/.nsmeta` y consumidor exit 42;
ocho negativas dirigidas. Sin baseline ni CTest general.
