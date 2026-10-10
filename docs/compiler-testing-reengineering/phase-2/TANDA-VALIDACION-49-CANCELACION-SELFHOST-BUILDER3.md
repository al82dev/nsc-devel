# Tanda de Validación 49: cancelación y timeout de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defecto demostrado

`ProcessController` devolvía un exit code después de cancelar o agotar el
timeout, pero no conservaba la causa por separado. `_run_variant` sólo convertía
una cancelación cuando el ejecutor ya había devuelto `INFRASTRUCTURE_ERROR`.
Un resultado parcial `FAIL` o `KNOWN_FAILURE` podía sobrevivir como si fuera un
defecto del compilador.

## Corrección mínima

`ProcessController.last_termination_reason` registra `cancelled` o `timeout`
independientemente del exit code. Cada variante limpia esa causa al empezar y,
al cerrar:

- una cancelación produce siempre `CANCELLED`;
- un timeout produce siempre `INFRASTRUCTURE_ERROR`;
- la evidencia ya creada por el ejecutor se conserva.

## Evidencia

- Cancelación previa y timeout con dobles: causa explícita preservada.
- Cancelación sobre resultado parcial `KNOWN_FAILURE`: `CANCELLED`.
- Timeout sobre resultado parcial `KNOWN_FAILURE`: `INFRASTRUCTURE_ERROR`.
- Pruebas dirigidas de terminación y clasificación: `8/8` PASS.
- Suite Python completa del controlador: `156/156` PASS.

No se creó ni terminó ningún proceso real, no se preparó ningún fixture, no se
abrió la GUI y no se configuró, compiló o ejecutó producto. No hubo baseline,
despliegue ni escritura en `C:`.
