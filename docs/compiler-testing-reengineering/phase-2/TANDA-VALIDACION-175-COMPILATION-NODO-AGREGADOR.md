# Tanda de validación 175 — Nodo agregador sin fuentes

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN DE BUILD Y PRUEBA DIRIGIDA PENDIENTE.

## SCOPE LOCK

Esta tanda corrige exclusivamente `TF-COMPILATION-334`, variante 6 del
contrato `CF-COMPILATION-042`.

Dentro del alcance:

- retirar de esa variante la fuente que contradice su propio oráculo;
- exigir materialización, finalización vacía y la referencia canónica a
  `Dep.nsproj`;
- sincronizar plan, inventario y manifiesto del caso.

Fuera del alcance:

- producto, Builder 3, `TRACKING.md` y cualquier otro caso;
- cambiar la política compartida de `ProjectLoader` o `ProjectSourceSet`;
- compilar o ejecutar pruebas en este commit.

## Evidencia de partida

La corrida `20261010-170051-1caf1c2f` conserva
`TF-COMPILATION-334|Debug|NA` como `FAIL`, contrato
`CF-COMPILATION-042`, con el detalle «materialization applies defaults and
permits only referenced aggregate nodes empty».

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

La variante escribía `Main.ns` porque la condición sólo excluía la variante
3. Después el mismo oráculo exigía que `sources()` estuviera vacío. El
producto sí implementa el contrato documentado: un proyecto sin fuentes es
válido cuando declara al menos un `ProjectReference`.

## Corrección causal

La variante 6 deja de crear `Main.ns`, mantiene un `Dep.nsproj` existente y
su `ProjectReference`, y exige:

- proyecto materializado;
- source set finalizado y vacío;
- exactamente una referencia;
- filename canónico `Dep.nsproj`.

Las otras siete variantes conservan su fixture y oráculo.

## Archivos autorizados

- `tests/compiler-suite/host/modules/CompilationProject217To336Module.cpp`
- `docs/compiler-testing-reengineering/test-plans/HOST-COMPILATION-TEST-PLAN.md`
- `docs/compiler-testing-reengineering/inventory/test-case-design.csv`
- `tests/compiler-suite/manifest/cases.jsonl`
- `docs/compiler-testing-reengineering/phase-2/TANDA-VALIDACION-175-COMPILATION-NODO-AGREGADOR.md`

## Validación pendiente

Por instrucción expresa no se compiló ni ejecutó. La validación posterior debe
construir `nsc_component_tests` en Debug y ejecutar exclusivamente
`TF-COMPILATION-334|Debug|NA`. Este documento no afirma PASS anticipado.
