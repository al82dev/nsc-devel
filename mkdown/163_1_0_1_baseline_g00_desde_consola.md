# NativeSharp 1.0.1 — baseline G00 desde consola

## Objetivo

Permitir que el baseline certificado que ofrece la primera pestaña de `nscbuilder.py`
también pueda ejecutarse sin abrir la interfaz gráfica:

```powershell
python nscbuilder.py --baseline --jobs 16
```

La entrada de consola no define una matriz alternativa. Construye el mismo `RunPlan` y
usa el mismo `PipelineWorker` que la interfaz gráfica, por lo que conserva las etapas,
los logs, el reporte histórico, la instalación del compilador y la certificación G00.

## Archivos modificados

### `nscbuilder.py`

- Se documentó `--baseline [--jobs N]` en la ayuda principal.
- Se agregó `make_baseline_plan`, fábrica única del plan inmutable de G0.2.
- `MainWindow._make_plan("baseline")` ahora delega en esa fábrica.
- Se agregó `run_baseline_cli`, que conecta la salida y el cierre de etapas del
  `PipelineWorker` a la consola.
- `main` reconoce `--baseline` y `--jobs`, valida un rango de 1 a 128 y evita crear una
  aplicación Qt cuando la ejecución es automatizada.

No se creó otro runner ni un script abreviado: cambiar la matriz certificada sigue
requiriendo modificar una sola definición.

## Funciones e implementación

### `make_baseline_plan(jobs, ...)`

Recibe las rutas configurables que ya usaba la GUI y produce el plan completo:

- construcción e instalación incremental de `nsc`;
- construcción de los dos árboles de pruebas;
- CTest con `lld-link` y NSLink;
- fixtures en `-O0` y `-O2`;
- triage con ambos enlazadores;
- sincronización de bibliotecas;
- medición y certificación Gate 0.

Los valores predeterminados corresponden al checkout canónico. La GUI conserva sus
campos editables pasándolos explícitamente a esta misma función.

### `run_baseline_cli(jobs)`

Ejecuta sincrónicamente el `PipelineWorker`, imprime cada etapa y devuelve un código
estable para automatización:

- `0`: reporte final con estado `passed`;
- `1`: baseline ejecutado pero fallido;
- `2`: argumentos inválidos.

Los mensajes generales que la GUI publica en sus dos canales se eliminan solamente de
la presentación consecutiva en consola. Los logs canónicos del runner no se alteran.

## Razón de diseño

La ejecución rápida y el baseline deben observar exactamente el código que está en el
disco. Compartir el plan y el worker evita que la ruta automatizada quede desfasada de
la pestaña del compilador o certifique una matriz parcial por accidente.

Los dos agregados CTest no deben lanzarse manualmente en paralelo: comparten el
directorio temporal `%TEMP%\nsc-cc1-rest`. El baseline oficial ya serializa ambos
enlazadores, así que no presenta esa colisión.

## Verificación

- `python nscbuilder.py --help`: muestra el uso automatizado.
- `python nscbuilder.py --baseline --jobs 0`: rechaza el valor con código `2`.
- Ejecución completa real:
  `python nscbuilder.py --baseline --jobs 16`.
- Corrida certificada: `20260908-210330-3a9390d3`.
- Resultado: 306/306 CTest, 148/148 fixtures y 18/18 casos de triage con cada
  enlazador; Gate 0 `LANDED`.
- Reporte:
  `_nscbuilder/history/20260908-210330-3a9390d3/report.md`.

## Referencia de commit / Commit reference

**Título ES:** `Habilita el baseline G00 desde consola`

**English title:** `Enable the G00 baseline from the command line`

**Descripción ES:**

> Comparte el plan G0.2 entre la interfaz gráfica y la consola, permite seleccionar el
> paralelismo y devuelve códigos de salida aptos para automatización. Conserva la matriz
> completa, los reportes históricos y la certificación Gate 0 del runner existente.

**English description:**

> Share the G0.2 plan between the GUI and command line, allow selecting parallelism,
> and return automation-friendly exit codes. Preserve the complete matrix, historical
> reports, and Gate 0 certification from the existing runner.
