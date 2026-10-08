# Tanda 1 — Matriz de destino de las pruebas existentes

## Resultado

- Casos inventariados: **684**.
- Casos con propietario resuelto: **684**.
- Casos con propietario no resuelto: **0**.
- Decisiones vacías: **0**.
- IDs duplicados: **0**.
- `KEEP_CPP_UNIT`: **59**.
- `MIGRATE_NS_FIXTURE`: **556**.
- `KEEP_SPECIALIZED`: **69**.
- SHA-256 del inventario fuente: `9b1c57d2f7e462a731a12b77775753a68985a33c590227baa8708983b39cae1d`.
- SHA-256 de la matriz: `f25927c829390bab94c817cb46245e87cbff8cd7d36b652cac4760c2ddc8f16e`.

## Regla aplicada

- Una invocación directa de NSC migra a fixture externo.
- Un owner C++ migra cuando contiene o referencia entrada NativeSharp demostrable.
- Un owner C++ sin entrada NativeSharp demostrada permanece como candidato unitario.
- Los verificadores CMake y Python multietapa permanecen especializados.
- Ningún caso se marca duplicado sin equivalencia demostrada.

## Archivos creados

- `tools/generate_test_destination_matrix.py`: generador estático, iterativo y reproducible.
- `docs/compiler-optimization/test-infrastructure/test-destination-matrix.json`: matriz canónica completa.
- `docs/compiler-optimization/test-infrastructure/test-destination-matrix.csv`: vista tabular completa.
- `docs/compiler-optimization/test-infrastructure/SCOPE-LOCK.md`: frontera autorizada de las 20 tandas.
- `docs/compiler-optimization/test-infrastructure/TRACKING.md`: estado persistente de las tandas.
- `docs/compiler-optimization/test-infrastructure/TANDA-TEST-INFRA-01-DESTINATION-MATRIX.md`: este reporte.

## Campos conservados por caso

Cada registro conserva ID CTest, componente, nivel, owner, ruta y hash del owner, comando completo,
entradas NativeSharp encontradas, contrato de entrada, señales de expectativa, contrato de expectativa,
decisión, destino futuro, fundamento, estado de migración y bloqueos.

## Validación ejecutada

```text
python tools\generate_test_destination_matrix.py
  --repo A:\nsc-devel
  --inventory A:\nsc-refactor-docs\implementacion\pruebas\12-INVENTARIO-CTEST-20261008.json
  --output A:\nsc-devel\docs\compiler-optimization\test-infrastructure
```

El generador terminó con código cero, resolvió los 684 owners, no encontró IDs duplicados ni
destinos vacíos y produjo el mismo hash en dos generaciones consecutivas.

## Comportamiento preservado

No se modificó ninguna fuente de prueba, expectativa, comando CTest, archivo CMake, fuente del
compilador ni artefacto construido. La tanda sólo añade inventario y documentación.

## Pendiente deliberado

La Tanda 2 creará el esqueleto del orquestador. Ningún caso se moverá hasta las tandas de migración
y ninguna representación antigua se retirará antes de su gate de paridad.

## Estado

Esta tanda clasifica destinos. No mueve fuentes, no modifica CMake, no ejecuta NSC y no retira pruebas.
La verificación de paridad pertenece a la tanda que migre cada caso.
