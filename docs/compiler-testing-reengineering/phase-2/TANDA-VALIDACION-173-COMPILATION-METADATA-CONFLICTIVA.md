# Tanda de validación 173 — Descriptor de metadata conflictivo

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN DE BUILD Y PRUEBA DIRIGIDA PENDIENTE.

## SCOPE LOCK

Esta tanda corrige exclusivamente `TF-COMPILATION-100`, consumidor del
contrato `CF-COMPILATION-013`.

Dentro del alcance:

- sustituir la precondición de filesystem que el mutador no posee por una
  relación inválida que sí pertenece a `ProjectGraph`;
- exigir el diagnóstico de identidades conflictivas y ausencia de mutación
  parcial;
- sincronizar contrato, plan, inventario y manifiesto para ese único caso.

Fuera del alcance:

- producto, Builder 3, `TRACKING.md` y cualquier otro caso;
- hacer que `ProjectGraph` lea o valide el contenido de archivos `.nsmeta`;
- compilar o ejecutar pruebas en este commit.

## Evidencia de partida

La corrida `20261010-170051-1caf1c2f` conserva
`TF-COMPILATION-100|Debug|NA` como `FAIL`, contrato
`CF-COMPILATION-013`, con el detalle «invalid owner, metadata, or duplicate
module is rejected».

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

La variante 4 entregaba `Missing.nsmeta` y esperaba rechazo por inexistencia.
`ProjectGraph::addPrecompiledProjects` recibe descriptors después del
descubrimiento de metadata: no abre esos paths. Su frontera es in-memory y sí
rechaza que un mismo `metadataPath` sea asignado a identidades de módulo
distintas.

## Corrección causal

La variante construye dos descriptors con el mismo owner y path resuelto, y
con módulos `Core` y `Graphics`. El oráculo exige:

- resultado negativo;
- texto `assigned conflicting module identities`;
- un solo proyecto restante;
- `App` conservado como root.

Las variantes de owner desconocido, módulo duplicado y éxito conservan sus
entradas y conducta.

## Archivos autorizados

- `tests/compiler-suite/host/modules/CompilationCore001To112Module.cpp`
- `docs/compiler-testing-reengineering/components/HOST-COMPILATION.md`
- `docs/compiler-testing-reengineering/test-plans/HOST-COMPILATION-TEST-PLAN.md`
- `docs/compiler-testing-reengineering/inventory/test-case-design.csv`
- `tests/compiler-suite/manifest/cases.jsonl`
- `docs/compiler-testing-reengineering/phase-2/TANDA-VALIDACION-173-COMPILATION-METADATA-CONFLICTIVA.md`

## Validación pendiente

Por instrucción expresa no se compiló ni ejecutó. La validación posterior debe
construir `nsc_component_tests` en Debug y ejecutar exclusivamente
`TF-COMPILATION-100|Debug|NA`. Este documento no afirma PASS anticipado.
