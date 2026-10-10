# Validacion 146 — Usabilidad del historial en Builder 3

## SCOPE LOCK

- Objetivo: hacer reconocible y accionable una corrida guardada sin mezclarla
  con el espacio de ejecucion vivo.
- Naturaleza: correccion de usabilidad de infraestructura.
- Archivos funcionales autorizados: `nscbuilder3.py`.
- Comportamiento preservado: carga inmutable desde evidencia, filtros,
  aislamiento del flujo vivo y creacion de una corrida nueva al reejecutar.
- Protegido: compilador, casos, manifiestos, ejecutores, bateria historica,
  CMake, despliegue y unidad `C:`.
- Validacion autorizada: sintaxis Python, smoke no interactivo de GUI y
  diferencial. No se compila ni ejecuta producto.

## Causa

La implementacion podia cargar y reejecutar evidencia, pero no presentaba un
flujo reconocible. La lista no explicaba fecha, configuracion, conteos u origen;
los artefactos de reporte aparecian al mismo nivel que las pruebas; y la
reejecucion dependia de descubrir una celda de accion.

## Implementacion

- La lista de historial muestra fecha, estado, conteos, configuracion, cantidad
  de resultados terminales, tipo, identificador y directorio.
- Seleccionar una fila ofrece vista previa de resumen, log y reporte antes de
  abrirla.
- Cada corrida abierta contiene dos secciones principales: `Pruebas` y
  `Reporte`.
- `Reporte` agrupa como tabs internos resumen, errores condensados, log
  completo, eventos, `results.json` y metadatos.
- `Pruebas` expone acciones visibles para mostrar fallos, reejecutar la prueba
  seleccionada o reejecutar la corrida completa.
- Toda reejecucion usa la configuracion actual de herramientas, crea evidencia
  nueva y conserva inmutable el historial consultado.
- El estado ocupado desactiva las acciones que iniciarían otra ejecucion.

## Aseguramiento

- `python -m py_compile nscbuilder3.py`: PASS.
- `python nscbuilder3.py --gui-smoke`: PASS.
- El smoke comprobo la jerarquia `Pruebas`/`Reporte`, los seis tabs internos,
  las dos acciones de reejecucion y el aislamiento del espacio vivo.
- No se compilo ni ejecuto producto, no se ejecuto baseline, no se desplego y
  no se toco `C:`.

## Estado

Implementacion lista para inspeccion visual del usuario. La tarea persistente
continua con la clasificacion causal de la corrida funcional.
