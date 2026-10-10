# Tanda de Validacion 101: duracion del adaptador host

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`nsc_component_tests` siempre publica `duration_us`, y la ruta Builder 3 ya lo
trataba como parte obligatoria del sobre. El adaptador canonico `HostExecutor`
ignoraba el campo: aceptaba su ausencia, booleanos, tipos ajenos y valores
negativos como si el resultado estuviera completo.

## Correccion

- `duration_us` exige tipo `int` exacto y valor no negativo;
- la ausencia y las coerciones quedan clasificadas como
  `INFRASTRUCTURE_ERROR`;
- los fixtures sinteticos reproducen ahora el sobre real del productor;
- `CaseResult.duration_seconds` conserva la medicion monotona del controlador,
  por lo que el dato del productor se valida pero no sustituye al reloj externo.

## Evidencia ejecutada

- pruebas dirigidas de `HostExecutor`: `12/12` PASS;
- suite Python completa del controlador: `235/235` PASS;
- ausencia, `true`, `-1`, `1.5` y `"42"` son rechazados;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra el esquema de duracion del adaptador host canonico. No
acredita una corrida funcional del producto. La meta permanece activa hasta
cero errores inesperados en la matriz acordada.
