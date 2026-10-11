# Tanda de validación 176 — Diagnóstico de selección SDK

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN DE BUILD Y PRUEBA DIRIGIDA PENDIENTE.

## SCOPE LOCK

Esta tanda corrige exclusivamente `TF-COMPILATION-381`, variante 5 del
contrato `CF-COMPILATION-048`.

Dentro del alcance:

- distinguir una versión malformada de una versión válida sin target;
- exigir los diagnósticos completos en las tres variantes negativas del
  contrato para conservar la frontera;
- sincronizar contrato, plan, inventario y manifiesto del caso 381.

Fuera del alcance:

- producto, Builder 3, `TRACKING.md` y cualquier otro caso;
- cambiar `SdkTargetSelector` o la identidad `NSC-SDK-003`;
- compilar o ejecutar pruebas en este commit.

## Evidencia de partida

La corrida `20261010-170051-1caf1c2f` conserva
`TF-COMPILATION-381|Debug|NA` como `FAIL`, contrato
`CF-COMPILATION-048`, con el detalle «selector returns first compatible range
or exact stable diagnostic».

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

La variante 5 pasa `bad` como versión y esperaba `NSC-SDK-003`. El productor
reserva `NSC-SDK-003` para arquitectura y versión sintácticamente válidas que
no encuentran rango. Una versión malformada se rechaza antes con
`invalid SDK target architecture or version`.

## Corrección causal

El oráculo exige el diagnóstico completo según la frontera:

- variante 3: `NSC-SDK-003` para `x86`/`10.2`;
- variante 4: `NSC-SDK-003` para `x64`/`12.0`;
- variante 5: `invalid SDK target architecture or version` para `x64`/`bad`;
- variantes exitosas: selección no nula y error vacío.

No cambia ninguna entrada ni conducta del selector.

## Archivos autorizados

- `tests/compiler-suite/host/modules/CompilationSdk337To384Module.cpp`
- `docs/compiler-testing-reengineering/components/HOST-COMPILATION.md`
- `docs/compiler-testing-reengineering/test-plans/HOST-COMPILATION-TEST-PLAN.md`
- `docs/compiler-testing-reengineering/inventory/test-case-design.csv`
- `tests/compiler-suite/manifest/cases.jsonl`
- `docs/compiler-testing-reengineering/phase-2/TANDA-VALIDACION-176-COMPILATION-DIAGNOSTICO-SDK.md`

## Validación pendiente

Por instrucción expresa no se compiló ni ejecutó. La validación posterior debe
construir `nsc_component_tests` en Debug y ejecutar exclusivamente
`TF-COMPILATION-381|Debug|NA`. Este documento no afirma PASS anticipado.
