# Tanda de Validación 31: despacho cerrado de ejecutores de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defecto demostrado

`ExecutionEngine._run_variant` sólo comprobaba explícitamente
`nsc_component_tests`. Cualquier otro ejecutor podía caer en una rama elegida
por el componente y, finalmente, en identidad. Un caso sintético con el valor
válido de esquema pero no implementado `historical-weekly` invocó identidad y
terminó `PASS`.

## Corrección mínima

Antes de especializar por componente, el motor exige que la segunda autoridad
sea exactamente `nsc_suite_controller+nsc`. Cualquier otro ejecutor lanza un
error que el propio motor conserva como `INFRASTRUCTURE_ERROR`.

No se cambió el enrutamiento vigente:

- `nsc_component_tests` -> host consolidado;
- `nsc_suite_controller+nsc` + `SelfHost.*` -> self-host;
- `nsc_suite_controller+nsc` + `BuildSystem` -> build system;
- `nsc_suite_controller+nsc` + `Packaging` -> packaging;
- los demás casos del controlador NSC -> identidad.

## Evidencia

- Filas ejecutables vivas inspeccionadas: `5,695`.
- Filas vivas sin ruta: `0`.
- Filas vivas con más de una ruta: `0`.
- Prueba negativa antes del cambio: `PASS` incorrecto.
- Prueba negativa después del cambio: `INFRASTRUCTURE_ERROR` correcto.
- Pruebas dirigidas: `2/2` PASS.
- Suite Python completa del controlador: `88/88` PASS.

No se configuró, compiló ni ejecutó producto; los controladores fueron dobles
sin procesos. No hubo baseline, despliegue ni escritura en `C:`.
