# Tanda de Validación 40: cancelación de comandos de Builder 3

Fecha: 10 de octubre de 2026
Estado: CERRADA CON CORRECCIÓN DIRIGIDA

## Defectos demostrados

`CommandWorker` convertía en `FAIL` el comando activo cancelado y omitía los
comandos todavía no iniciados. Una cancelación anterior al primer comando dejaba
cero resultados y no podía publicar un cierre terminal.

## Corrección mínima

Dos proyecciones puras generan resultados de infraestructura:

- el comando activo recibe `CANCELLED` cuando el controlador fue cancelado;
- cada comando no iniciado recibe una fila `CANCELLED` con su clave y etiqueta;
- el orden y la cardinalidad siguen exactamente el plan original.

`CommandWorker` usa esas proyecciones sin cambiar comandos ni terminación de
procesos.

## Evidencia

- Comando activo cancelado: `CANCELLED`, no `FAIL`.
- Cancelación después del primer comando: dos filas terminales restantes.
- Cancelación antes del primer comando: `3/3` filas terminales.
- Pruebas dirigidas: `3/3` PASS.
- Suite Python completa del controlador: `119/119` PASS.

No se crearon procesos reales, no se abrió la GUI y no se configuró, compiló o
ejecutó producto. No hubo baseline, despliegue ni escritura en `C:`.
