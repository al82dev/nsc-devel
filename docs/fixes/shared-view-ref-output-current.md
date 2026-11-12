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

Un control posterior amplió la entrada escalar a `in SharedView` y conservó
salidas `ref` y `out`. El productor se analizó y publicó en O0/O2 y ambas
firmas binarias se materializaron en cada nivel. Pasaron 48 análisis de
consumidores: 16 usos vivos aceptados, 16 escapes y 16 reemplazos rechazados
con diagnósticos de vida; la mitad de cada grupo usó `Sink` binario. Los 16
casos vivos ejecutaron 42 y los inseguros solo se analizaron. Evidencia:
`A:/nsc-fixes-agente-2-evidencia/binary-view-ref-out-20261111/run-20261112T033725649320Z/results.json`.
Se mantuvieron el Core preexistente y el compilador experimental indicados
arriba. No se probaron tuplas, DLL, niveles cruzados ni metadata histórica.

Otra matriz examinó dos entradas y el uso del mismo local como entrada `ref` y
salida. Pasaron 112 comprobaciones semánticas esperadas con Rc/Arc, mitad con
productor fuente y mitad binario: se aceptaron 32 casos vivos y se rechazaron
80 escapes o reemplazos con diagnóstico de vida. Se ejecutaron 24 casos vivos
y devolvieron 42. Los ocho casos vivos con el mismo local como `ref` de entrada
y `out` de salida se aceptaron semánticamente, pero no se ejecutaron: aún no se
ha demostrado cómo interactúa la inicialización de `out` con la entrada
aliasada. Tampoco se ejecutaron los casos inseguros. Evidencia y comandos:
`A:/nsc-fixes-agente-2-evidencia/binary-view-multiple-20261112/run-20261112T034051540100Z/results.json`.
El compilador contenía cambios experimentales y Core era binario preexistente;
no se prueban tuplas, DLL, niveles cruzados ni metadata histórica.

Un control seguro separado con `int` pasó O0/O2 cuando `ref` y `out` apuntan
al mismo local: en los casos probados la lectura observó el valor inicial hasta
que el método escribió, y las escrituras posteriores fueron visibles por ambos
parámetros. Comandos y resultados:
`A:/nsc-fixes-agente-2-evidencia/ref-out-same-scalar-20261112/run-20261112T034407562284Z/results.json`.
Esto no establece el contrato general ni la seguridad equivalente para
`SharedView` o métodos binarios; esa ejecución continúa sin verificarse.
