# NS3-T00-F05 — Catálogo estable de diagnósticos

## Resultado

Los diagnósticos normativos iniciales de 3.0 forman un catálogo explícito: `NS3001`, `NS3002`,
`NS3010`, `NS3020`, `NS3030`, `NS3049` y `NS3099`. El renderer no identifica el catálogo por una
coincidencia textual: consulta `isNativeSharp3Diagnostic`, preservando los IDs heredados sin código
entre corchetes. Error, warning, lint y note mantienen contadores y representación separados.

## Cambios

- se añadió la consulta constexpr del catálogo 3.0 completo;
- el renderer usa la pertenencia explícita al catálogo;
- se incorporó `NS3049` al índice normativo, que antes estaba implementado pero omitido;
- el índice ES/EN ahora documenta causa, ejemplo mínimo y corrección para cada código;
- la prueba constitucional comprueba que un ID heredado no se clasifica como `NS3xxx`.

## Prueba dirigida

- objetivo: `nsc_ns3_t00_constitution_tests`
- resultado: `PASS: 32 checks`
- cobertura: severidades independientes, primary/secondary spans, notes, render de `NS3030`,
  pertenencia de `NS3049` y exclusión de IDs heredados.
- baseline: no ejecutado.

## Archivos modificados

- `include/nativesharp/Diagnostics/DiagnosticId.h`
- `src/Diagnostics/DiagnosticRenderer.cpp`
- `tests/unit/NativeSharp3T00ConstitutionTests.cpp`
- `docs/3.0/TANDA-00-CONSTITUCION-DEL-LENGUAJE.md`

## Mejora

Herramientas, snapshots y usuarios pueden depender de códigos estables sin que un cambio de texto o
un nombre heredado altere su clasificación.
