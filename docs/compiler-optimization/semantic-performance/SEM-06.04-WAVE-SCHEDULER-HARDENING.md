# SEM-06.04 — Endurecimiento del scheduler de oleadas

## Causa

La primera integración paralela conservaba el orden normal, pero cuatro fronteras de fallo no
estaban congeladas completamente: una cancelación previa todavía permitía ejecutar la oleada,
`j1` y `jN` publicaban textos de error distintos, la limpieza de un fallo de `submit` silenciaba
una segunda excepción y una instancia podía recibir dos coordinadores simultáneos.

## Cambios

- La cancelación se consulta en el coordinador antes de ejecutar o enviar cualquier callable.
- El fallo de análisis usa el mismo contrato observable en el camino síncrono y paralelo.
- La limpieza tras un fallo de scheduling conserva por separado el error principal y el error
  secundario de cierre; no existe un `catch` vacío.
- La inicialización fallida del pool conserva su `exception_ptr` para el diagnóstico posterior.
- Un guard atómico rechaza una segunda llamada concurrente a `run` sin ejecutar su callback.
- El fast path se vuelve a decidir en cada `run`; un scheduler creado fuera de un worker tampoco
  crea paralelismo anidado si después se invoca desde un worker de compilación.
- El header documenta que `Analyze` debe ser reentrante y operar exclusivamente sobre shards y
  slots aislados.

## Archivos

- `include/nativesharp/Frontend/ProjectCallableWaveScheduler.h`
- `src/Frontend/ProjectCallableWaveScheduler.cpp`
- `tests/unit/ProjectCallableWaveSchedulerTests.cpp`
- `docs/compiler-optimization/semantic-performance/SEM-06.01-BOUNDED-WAVE-SCHEDULER.md`

No se crearon nuevos tipos de producto. Este documento editorial es nuevo porque el commit debe
explicar el riesgo concurrente, su ownership y la evidencia que lo cierra.

## Complejidad y ownership

El trabajo sigue siendo `O(C)` por oleada y usa `O(C)` slots, donde `C` es el número de callables
elegibles. No hay recursión. Cada worker escribe un único slot y el coordinador es el único que
consulta cancelación, publica resultados o cambia el estado de ejecución del scheduler.

## Evidencia dirigida

Comando:

```powershell
cmake --build A:\nsc-devel\build-test-nslink `
  --target nsc_project_callable_wave_scheduler_tests --parallel 12
A:\nsc-devel\build-test-nslink\tests\nsc_project_callable_wave_scheduler_tests.exe
```

Resultado: `PASS: 12 checks`.

`clang-tidy` se ejecutó con `clang-analyzer-*`, `bugprone-*` y `performance-*`. Encontró que no
podía demostrar la precondición de un `optional` en la segunda pasada transaccional; se añadió una
aserción del invariante y una supresión local justificada: fusionar validación y publicación
permitiría resultados parciales, que el contrato prohíbe. Los demás diagnósticos pertenecen a
headers preexistentes y al harness compartido; no se presentan como una pasada global limpia.

También pasaron `git diff --check` y la comprobación de líneas mayores a 120 caracteres sobre los
tres archivos C++ afectados. No se ejecutaron baseline, minibaseline, CTest general ni O1/O2/O3.
