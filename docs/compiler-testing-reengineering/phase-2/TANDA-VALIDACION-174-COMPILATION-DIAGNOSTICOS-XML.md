# Tanda de validación 174 — Diagnósticos XML exactos

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN DE BUILD Y PRUEBAS DIRIGIDAS PENDIENTE.

## SCOPE LOCK

Esta tanda corrige exclusivamente `TF-COMPILATION-209..216`, las ocho
variantes de `CF-COMPILATION-027`.

Dentro del alcance:

- sustituir el formato inventado `line`/`column` por el formato público
  `XML <línea>:<columna>: <mensaje>`;
- exigir el primer diagnóstico completo y exacto de cada fixture;
- sincronizar contrato, plan, inventario y manifiesto de esos ocho casos.

Fuera del alcance:

- producto, Builder 3, `TRACKING.md` y casos ajenos a `209..216`;
- cambiar el parser o su formato vigente;
- compilar o ejecutar pruebas en este commit.

## Evidencia de partida

La corrida `20261010-170051-1caf1c2f` conserva los ocho IDs como `FAIL` con
el detalle «parser reports one deterministic first error with line and
column». El envelope no publicó los strings intermedios.

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

El oráculo buscaba literalmente `line ` y `column `. El productor
`SimpleXmlParser::location` genera `XML L:C`; `SimpleXmlParser::fail` sólo
publica cuando el error está vacío, por lo que conserva el primer fallo.

## Corrección causal

Cada variante conserva su source y ahora compara el diagnóstico completo:

| Caso | Diagnóstico |
|---|---|
| 209 | `XML 1:8: closing element 'B' does not match 'A'` |
| 210 | `XML 4:9: closing element 'C' does not match 'B'` |
| 211 | `XML 1:11: duplicate XML attribute 'x'` |
| 212 | `XML 1:5: unsupported XML entity '&bad;'` |
| 213 | `XML 1:7: element 'B' is not closed` |
| 214 | `XML 3:4: expected a quoted XML attribute value` |
| 215 | `XML 1:12: closing element 'Wrong' does not match 'A'` |
| 216 | `XML 1:8196: element 'A' is not closed` |

## Archivos autorizados

- `tests/compiler-suite/host/modules/CompilationXml169To216Module.cpp`
- `docs/compiler-testing-reengineering/components/HOST-COMPILATION.md`
- `docs/compiler-testing-reengineering/test-plans/HOST-COMPILATION-TEST-PLAN.md`
- `docs/compiler-testing-reengineering/inventory/test-case-design.csv`
- `tests/compiler-suite/manifest/cases.jsonl`
- `docs/compiler-testing-reengineering/phase-2/TANDA-VALIDACION-174-COMPILATION-DIAGNOSTICOS-XML.md`

## Validación pendiente

Por instrucción expresa no se compiló ni ejecutó. La validación posterior debe
construir `nsc_component_tests` en Debug y ejecutar exclusivamente
`TF-COMPILATION-209..216|Debug|NA`. Este documento no afirma PASS anticipado.
