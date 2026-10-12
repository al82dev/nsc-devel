# NS304-T00-F08 — Harness estructural 3.0.4

## Estado

`CERRADO` el 2026-10-12. El harness descubre su universo desde las fuentes
canónicas y produce resultados JSON estructurados; no ejecuta producto.

## SCOPE LOCK

- Objetivo: detectar drift entre plan, tracking y contratos 3.0.4 sin
  denominadores, hashes de herramientas ni mensajes humanos congelados.
- Implementación: `tools/verify_native_sharp_304_plan.py`.
- Pruebas: `tests/unit/test_verify_native_sharp_304_plan.py`.
- Revisión base: `f1d1a69db` en `development`.
- Protegido: producto, runtime, CMake, toolchain, Builder, suite y configuración.

## Contrato observable

Entrada obligatoria:

1. plan canónico Markdown;
2. tracking maestro Markdown;
3. directorio de contratos JSON 3.0.4.

Salida única JSON:

- `status`: `PASS` o `FAIL`;
- `discovered`: cantidades calculadas desde las entradas;
- `issues`: códigos estables, ruta e ID cuando aplica.

El harness no contiene `147`, hashes, versión/ruta de Python, LLVM o NSC, ni
compara textos explicativos completos. Los IDs se descubren sólo desde filas
canónicas; agregar un ítem al plan cambia el denominador y exige su fila de
tracking sin modificar el programa.

## Invariantes

- IDs del plan únicos y no vacíos.
- IDs del tracking únicos, completos, sin extras y en el mismo orden.
- cierre calculado desde checkboxes, nunca desde un número esperado.
- contratos JSON parseables, object root, schema `NS304_*`, schema version 1 y
  release `3.0.4`.
- fallos reportados por código (`TRACKING_MISSING_ITEM`,
  `CONTRACT_JSON_INVALID`, etc.), no por coincidencia con excepción/prosa.

## Evidencia puntual

- Unitarias: 4/4 `PASS` en 0.053 s.
- Casos: denominador dinámico; item nuevo sin tracking; errores contractuales
  estructurados; JSON inválido sin dependencia del texto de excepción.
- Ejecución real: `PASS`, 147 ítems descubiertos en plan, 147 en tracking,
  8 cerrados y 2 contratos cargados.
- El primer intento detectó un defecto del fixture de importación dinámica; se
  registró el módulo en `sys.modules` y la única repetición quedó limpia.

## Gate F08

```text
fixed_plan_denominator = absent
tool_hash_requirements = absent
fragile_message_oracles = absent
focused_unit_tests = 4/4 PASS
live_plan_tracking_contract_gate = PASS
```

## Consolidado T00

El consolidado puntual terminó `PASS`: F01=55, F02=55, F03=88, F04=69,
F05=7/7 casos, F06=6 contratos, F07=18 familias y F08=4/4 unitarias más gate
real. No se ejecutó batería completa.

T00 queda cerrada. La siguiente acción es `NS304-T01-F01`.
