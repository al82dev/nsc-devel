# Tanda NSCBuilder: consolas completas y reporte de errores en pestaña

## Problemas corregidos

- Las pestañas `Consola O0`–`Consola O3` permanecían vacías durante etapas compartidas porque la
  salida `General` sólo se enviaba al log general.
- El resumen condensado se abrió como diálogo modal y todavía exigía abrir cada log para consultar
  toda la evidencia.

## Cambios

- `Consola general` es ahora el agregado de toda la salida del canal.
- Cada consola de optimización seleccionada recibe el contexto compartido de la corrida y toda su
  salida específica; por tanto funciona como un log completo e independiente.
- Cada canal incorpora `Reporte de errores`, una pestaña permanente con causas agrupadas, casos
  afectados, diagnóstico principal y contenido completo de todos los logs causales.
- Los cierres derivados siguen visibles en `Errores` y `metadata.json`, pero no duplican causas en
  el reporte.
- Al terminar una corrida fallida se selecciona el reporte automáticamente. Ya no aparece una
  ventana modal ni es necesario hacer doble clic para leer la evidencia.

## Archivos

- Modificado: `nscbuilder.py`.
- Modificado: `tests/unit/test_nscbuilder_pipeline.py`.
- Nuevo: este documento.

## Validación

Sólo se ejecuta la prueba unitaria dirigida de NSCBuilder. No se ejecuta baseline, CTest general ni
matriz de optimizaciones.
