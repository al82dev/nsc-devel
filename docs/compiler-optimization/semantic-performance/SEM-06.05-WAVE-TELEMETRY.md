# SEM-06.05 — Telemetría barata por oleada

## Propósito

Hacer observable el costo real de `method-bodies` sin añadir un segundo sistema de trazas ni pagar
relojes en compilaciones normales. La implementación reutiliza el `phaseTimingSink` que el Driver
activa con `--time`.

## Contrato

Cuando existe un sink, cada oleada publica en el hilo coordinador:

- `method-bodies-wave-N-analysis`: pared completa del scheduler para la oleada;
- `method-bodies-wave-N-wait`: espera del coordinador en la barrera paralela;
- `method-bodies-wave-N-merge`: reducción y publicación estable de resultados.

El Driver ya asocia a cada fase `peak_working_set_bytes`, por lo que estas entradas conservan el
mismo muestreo de memoria que el resto del pipeline. Sin `--time`, el sink está vacío: no se toma
ningún reloj ni se construyen nombres de fase. El sink es coordinador-only y no puede lanzar, igual
que el contrato existente de `ProjectCompilation`.

`ProjectCallableAnalysisStatistics` añade sólo contadores baratos: workers solicitados y efectivos,
ancho máximo de oleada y número de oleadas síncronas/paralelas. Los contadores semánticos previos
siguen siendo independientes del número de jobs.

## Archivos

- `include/nativesharp/Frontend/ProjectCallableAnalysisStatistics.h`
- `include/nativesharp/Frontend/ProjectCallableWaveScheduler.h`
- `include/nativesharp/Frontend/ProjectMethodBodyBuildOptions.h`
- `src/Frontend/ProjectCallableWaveScheduler.cpp`
- `src/Frontend/ProjectCompilation.cpp`
- `src/Frontend/ProjectMethodBodyModel.cpp`
- `tests/unit/ProjectMethodBodyModelTests.cpp`

No se creó otro tipo ni otro subsistema de logging. Se amplió el contrato existente porque el
objetivo es medir la ruta productiva, no mantener una telemetría paralela sólo para pruebas.

## Complejidad y concurrencia

Los contadores son `O(1)` por oleada. La telemetría opcional realiza dos lecturas de reloj por fase
y una allocation corta para el nombre; nunca se ejecuta desde un worker. No hay recursión ni estado
mutable compartido nuevo.

## Evidencia dirigida

```powershell
cmake --build A:\nsc-devel\build-test-nslink `
  --target nsc_project_callable_wave_scheduler_tests `
           nsc_project_method_body_model_tests --parallel 12
A:\nsc-devel\build-test-nslink\tests\nsc_project_callable_wave_scheduler_tests.exe
A:\nsc-devel\build-test-nslink\tests\nsc_project_method_body_model_tests.exe
```

Resultados: `PASS: 12 checks` y `PASS: 76 checks`.

El fixture fuerza más de una oleada, compara `j1`, `j2` y `j12`, comprueba todos los contadores
semánticos y exige una entrada `analysis` y `merge` por oleada, más una entrada `wait` por oleada
paralela. No se ejecutaron baseline, minibaseline, CTest general ni O1/O2/O3.
