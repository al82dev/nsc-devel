# Tanda de Validacion 63: enteros de ruta sin coercion

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`run_buildsystem_gate.py` y `run_packaging_gate.py` seleccionaban contrato y
variante mediante `int(inputs[...])`. Esa conversion aceptaba entradas que el
contrato no declara: por ejemplo, `true` se convertia en `1` y podia ejecutar
una rama real en lugar de denunciar un manifiesto corrupto.

## Correccion

- `required_manifest_integer` exige presencia y tipo `int` exacto;
- `bool`, cadenas, flotantes, `null` y campos ausentes fallan antes del
  despacho;
- BuildSystem y Packaging comparten el mismo validador;
- no cambiaron contratos, variantes, comandos, oraculos ni manifiestos.

## Evidencia ejecutada

- pruebas dirigidas de soporte y rutas: `10/10` PASS;
- suite Python completa del controlador: `201/201` PASS;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra que los dos gates rechazan tipos coercibles antes de
seleccionar una ruta. No demuestra ejecucion funcional de BuildSystem,
Packaging, NSC o NSLink. La meta sigue activa hasta que la matriz funcional
acordada termine sin errores inesperados.
