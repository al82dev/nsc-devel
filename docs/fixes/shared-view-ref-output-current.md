# Estado de las salidas prestadas de vistas

Corrección de procedencia: las primeras matrices y controles descritos abajo
iniciaron procesos hijos desde temporales en A; se conservan como observaciones
no conformes y no se cuentan como validación conforme. Se corrigió el ejecutor
de 92 controles semánticos y se repitió desde el repositorio con `FAILURES []`:
`A:/nsc-fixes-evidencia/view-probes-repo-cwd-views-20261112.log`. La selección
de ocho ejecuciones válidas se repitió también desde el repositorio:
`A:/nsc-fixes-evidencia/view-output-valid-repo-cwd-20261112.cmd` y `.log`.
El archivo de preparación de la copia temporal se modificó para esa repetición;
su contenido actual no es idéntico al de la primera ejecución.

Las matrices binarias se repitieron desde el repositorio aislado del agente,
con procesos hijos en esa misma copia. La evidencia conforme está en
`A:/nsc-fixes-agente-2-evidencia/binary-view-cwd-repetition-20261112/`:
`ref-out/run-20261112T040726384706Z/results.json`,
`in/run-20261112T040743251503Z/results.json`,
`multiple/run-20261112T040752328809Z/results.json` y
`missing-metadata/run-20261112T040804419101Z/results.json`.
También se repitieron desde el repositorio del agente los análisis y emisiones
del alias de vista: `A:/nsc-fixes-agente-2-evidencia/sharedview-alias-cwd-repetition-20261112/`
contiene `run-20261112T041214200705Z/results.json` para semántica,
`run-20261112T041217980737Z/results.json` para IR,
`object-run-20261112T041218826410Z/results.json` para objetos,
`producer-run-20261112T041219824808Z/results.json` para el productor y
`observed-run-20261112T041220524462Z/results.json` para la lectura observable.
No hubo diferencias materiales con las observaciones de código descritas
abajo. Los dumps no demuestran un contrato general ni se ejecutó la forma
aliasada. El control escalar previo sigue siendo no conforme y no se cuenta
como validación hasta repetirlo.

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

Se inspeccionó sin ejecutar la forma `ref vista, out vista`. En los dumps LLVM
examinados el consumidor pasa la misma dirección a ambos parámetros sin una
escritura de cero intermedia, y las firmas mostradas no contienen `noalias`.
El productor de fuente examinado carga antes de guardar; las bibliotecas
estáticas concretas desensambladas hacen lo mismo. En el objeto del consumidor
binario, O0 y O2 preparan la misma dirección para ambos argumentos. El consumidor
fuente O0 hace lo mismo, pero en O2 la llamada fue eliminada porque el valor no
se observaba después: ese objeto no demuestra sus argumentos. Los dumps O0 y
O2 fueron idénticos y no se consideran prueba de IR optimizado distinto.
Comandos y resultados, incluidos productor y objetos:
`A:/nsc-fixes-agente-2-evidencia/sharedview-same-ref-out-ir-20261112/`.
Se usó el compilador experimental y productores binarios concretos; no se
ejecutó el caso ni se establece un contrato general para alias, otras ABI o DLL.

Con dos entradas `in` distintas y una salida `ref` u `out`, la selección
dirigida pasó 80 análisis: 16 casos vivos aceptados, 32 escapes y 32 reemplazos
rechazados para cada dueño. Los 16 casos vivos ejecutaron 42; no se ejecutaron
los inseguros. Mitad usó el productor fuente y mitad una biblioteca estática,
en O0/O2 con niveles iguales para productor y consumidor. Aunque la condición
escogía la primera entrada, el rechazo incluyó el escape o reemplazo de la
segunda, como espera la unión conservadora en estos casos. Evidencia:
`A:/nsc-fixes-agente-2-evidencia/binary-view-multiple-20261112/run-20261112T035306629567Z/results.json`.
El compilador seguía con WIP y Core era binario preexistente; no se cubren DLL,
niveles cruzados ni metadata histórica.

Un productor estático reciente se comprobó con su metadata coincidente y con
copias de la biblioteca sin metadata. Con metadata presente el caso vivo se
aceptó y el escape se rechazó con diagnóstico de vida. Sin metadata, tanto la
búsqueda implícita como una ruta explícita inexistente rechazaron los casos
antes de analizar sus vidas. Son controles de la barrera de importación, no
una prueba de contratos de productores históricos. No se ejecutaron
consumidores. Evidencia:
`A:/nsc-fixes-agente-2-evidencia/binary-view-no-metadata-20261112/run-20261112T035645240255Z/results.json`.

Un segundo objeto fuente O2 hizo observable la lectura posterior de `Data`.
El optimizador todavía eliminó la autocopia de `AliasOut`, pero conservó la
lectura del slot, su comprobación de null y el acceso al objeto antes de
liberar al dueño. No queda un sitio de llamada cuyos argumentos puedan
inspeccionarse en ese objeto. Evidencia, sin enlace ni ejecución:
`A:/nsc-fixes-agente-2-evidencia/sharedview-same-ref-out-ir-20261112/observed-run-20261112T035133432865Z/results.json`.
El resultado no convierte esa forma en contrato general.

Un control seguro separado con `int` pasó O0/O2 cuando `ref` y `out` apuntan
al mismo local: en los casos probados la lectura observó el valor inicial hasta
que el método escribió, y las escrituras posteriores fueron visibles por ambos
parámetros. Comandos y resultados:
`A:/nsc-fixes-agente-2-evidencia/ref-out-same-scalar-20261112/run-20261112T034407562284Z/results.json`.
Esto no establece el contrato general ni la seguridad equivalente para
`SharedView` o métodos binarios; esa ejecución continúa sin verificarse.
