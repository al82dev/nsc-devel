# Tanda de validación 186 — Capacidad instalada ausente

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN DE BUILD Y PRUEBA DIRIGIDA PENDIENTE.

## SCOPE LOCK

Esta tanda corrige exclusivamente `TF-DRIVER-027`, consumidor de
`CF-DRIVER-004`.

Dentro del alcance:

- construir el fixture incompleto desde la política canónica vigente;
- omitir sólo `language_release` y exigir su diagnóstico propietario;
- sincronizar plan, inventario y manifiesto para ese caso.

Fuera del alcance:

- producto, Builder 3, `TRACKING.md` y cualquier otro caso;
- alterar el orden o la política de validación de capacidades;
- compilar o ejecutar pruebas en este commit.

## Evidencia de partida

La corrida `20261010-170051-1caf1c2f` registra
`TF-DRIVER-027|Debug|NA` como `FAIL`, con el detalle «incomplete installed
capability policy is rejected».

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

El fixture artesanal declaraba `compiler_release=0`. El verificador recorre
las capacidades normativas en orden y, correctamente, rechazaba ese valor
antes de poder informar una capacidad ausente. Por tanto, la entrada no
aislaba la precondición que el caso decía probar.

## Corrección causal

El fixture se deriva de `currentCompatibilityPolicy()`, elimina únicamente la
línea `language_release` y conserva normativos todos los campos anteriores. El
oráculo exige el diagnóstico exacto de capacidad requerida ausente para ese
campo, sin aceptar un rechazo anterior por valor incompatible.

## Validación pendiente

Por instrucción expresa no se compiló ni ejecutó. La validación posterior debe
construir `nsc_component_tests` en Debug y ejecutar exclusivamente
`TF-DRIVER-027|Debug|NA`. Este documento no afirma PASS anticipado.
