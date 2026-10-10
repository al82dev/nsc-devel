# Tanda de Validación 39: integridad del historial de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defecto demostrado

La tabla de historial leía `status` directamente desde cualquier
`results.json`. Una corrida con metadata o resultados contradictorios podía
presentarse como `PASSED` porque la GUI no cotejaba el cierre completo.

## Corrección mínima

`history_record` valida, antes de proyectar la fila:

- `run_id` y tipo de corrida;
- presencia de resultados, resumen y eventos;
- resultados no vacíos con claves únicas y estados conocidos;
- cardinalidad y conteos coherentes;
- estado global derivado de los resultados;
- último evento `RUN_FINISHED` coherente con `results.json`.

La ausencia de cierre queda `INCOMPLETE`; una contradicción queda `MALFORMED`.

## Evidencia

- Corrida sintética terminal coherente: conserva `PASSED`.
- Corrida sin artefactos terminales: `INCOMPLETE`.
- Estado global desconocido: `MALFORMED`.
- Conteos contradictorios: `MALFORMED`.
- Pruebas dirigidas: `4/4` PASS.
- Suite Python completa del controlador: `116/116` PASS.
- Historial local auditado en sólo lectura: `17` corridas, `10 FAILED`,
  `7 PASSED`, `0` reclasificaciones incompletas o malformadas.

No se modificó el historial existente, no se abrió la GUI y no se configuró,
compiló o ejecutó producto. No hubo baseline, despliegue ni escritura en `C:`.
