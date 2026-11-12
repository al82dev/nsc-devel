# Estado de las salidas prestadas de vistas

El 11 de noviembre de 2026 `borrowed_tuple_ref_output.py` ejecutó 92 controles
semánticos con Rc y Arc y terminó con `FAILURES []`. De ellos, 12 son rechazos
conservadores esperados de salidas que contienen una tupla con una vista; no
se presentan como llamadas válidas soportadas. Los controles restantes
comparan aceptación y rechazo para ámbitos de dueños, reemplazos y salidas
directas o reenviadas. No se ejecutaron programas en esta selección.

Comando y salida:
`A:/nsc-fixes-evidencia/borrowed-tuple-output-current-20261111.cmd` y `.log`.
Se usaron el compilador incremental autorizado y Core binario aislado
preexistente. El árbol contiene cambios locales y el fixture sigue sin registrar;
esta ejecución no certifica el commit limpio ni valida metadata binaria
histórica. La protección de vistas compuestas en llamadas binarias continúa
activa y rechaza también lecturas válidas; el resumen de procedencia necesario
para habilitarlas sigue en investigación.

Como control de ejecución separado se preparó una copia temporal del mismo
fixture limitada a `live` y `out_live`, sin modificar el original. Las ocho
combinaciones de Rc/Arc, ref/out y O0/O2 compilaron y devolvieron 42. La
preparación, comando y log están en `A:/nsc-fixes-evidencia/` con prefijos
`prepare-view-output-runtime-control`, `borrowed-tuple-output-valid-control`
y `borrowed-tuple-output-valid-20261111`. El Core binario era preexistente y
los cambios de análisis seguían sin consolidar; no se ejecutaron ejemplos
inseguros ni se acredita el contrato general.

Un control adicional separó `Sink` en una biblioteca NativeSharp estática y
comprobó que sus cuatro firmas escalares de entrada por valor o `ref` y salida
`ref` u `out` se materializan. La matriz con Rc/Arc pasó 96 análisis: 32 usos
válidos aceptados, 32 escapes y 32 reemplazos rechazados con diagnósticos de
vida; cada grupo incluyó 16 variantes con `Sink` fuente y 16 con `Sink` binario.
Los 32 casos válidos compilaron y devolvieron 42; los inseguros no se
ejecutaron. La publicación del productor y sus consumidores se probó en O0/O2
del mismo nivel, sin cruzar niveles. Comandos, fuentes, hashes y salidas:
`A:/nsc-fixes-agente-2-evidencia/binary-view-ref-out-20261111/run-20261112T032759700080Z/results.json`.
El Core binario era preexistente y el compilador contenía cambios locales e
instrumentación experimental. La evidencia no cubre vistas dentro de tuplas,
entrada `in`, DLL, bibliotecas históricas ni descarga de módulos.
