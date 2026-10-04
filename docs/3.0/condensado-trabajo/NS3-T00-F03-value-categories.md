# NS3-T00-F03 — Categorías de valores

## Resultado

El pipeline productivo de proyectos publica una categoría canónica por expresión: `place`,
`value`, `temporary` o `borrowed-value`. Los estados moved-from permanecen en el análisis de flujo
y su lectura se rechaza con el código estable `NS3020`. Ownership y categoría de valor quedan
separados: ownership decide destrucción; la categoría decide almacenamiento, materialización y uso.

## Cambios

- `MethodExpressionTypeBinding` transporta `sema::ValueCategory` hacia lowering y tooling.
- los identificadores almacenados se publican como `place`.
- los aliases prestados se publican como `borrowed-value`.
- resultados struct no direccionables de llamadas, construcción, agregados, tuplas y `with` se
  publican como `temporary`.
- los demás resultados computados se publican como `value`.
- las lecturas de locales o parámetros moved-from usan `NS3020` tanto en flujo definitivo como en
  joins posiblemente movidos.

## Prueba dirigida

- objetivo: `nsc_stage49_raii_move_tests`
- resultado: `PASS: 85 checks`
- cobertura añadida: publicación real de `place`, `value`, `temporary` y `borrowed-value`; lectura
  después de move con `NS3020`; conservación del lowering move/cleanup hasta LLVM.
- baseline: no ejecutado.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
- `src/Frontend/ProjectMethodBodyModel.cpp`
- `tests/unit/Stage49RaiiMoveSemanticsTests.cpp`

## Mejora

HIR, diagnósticos, LSP y debugger pueden consumir la misma clasificación en vez de reconstruirla
desde flags de asignabilidad y ownership, eliminando una fuente de discrepancias semánticas.
