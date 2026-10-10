# Tanda de Validación 61: causas de productores self-host en Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defecto demostrado

La compilación de un fixture self-host reducía cuatro resultados distintos a
`failed to compile with exit code N`: timeout, cancelación, salida no cero y
salida cero sin ejecutable. En particular, un timeout quedaba cacheado sólo como
`exit code 130`; los consumidores posteriores perdían la causa del productor.

## Corrección mínima

`_prepare_selfhost_fixture` consulta la causa registrada por
`ProcessController` antes del exit code y publica diagnósticos separados para
timeout y cancelación. Después distingue fallo del compilador y ausencia del
artefacto requerido. `_run_selfhost` conserva ese texto en el cache causal, por
lo que cada consumidor muestra el mismo `BLOQUEADO_POR`.

## Evidencia

- Timeout y cancelación simulados: causas distintas conservadas.
- Exit code `7` y salida `0` sin ejecutable: diagnósticos distintos.
- Timeout del productor: primer caso `INFRASTRUCTURE_ERROR`; consumidor
  posterior `BLOCKED` con la causa exacta.
- Pruebas dirigidas de fixture y clasificación: `5/5` PASS.
- Suite Python completa del controlador: `199/199` PASS.

Todas las pruebas usaron dobles y un historial sintético; no se creó o ejecutó
ningún proceso real. No se modificaron fixtures, comandos u oráculos. No hubo
baseline, despliegue ni escritura en `C:`.
