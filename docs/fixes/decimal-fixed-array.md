# Arreglo fijo de decimal

El caso `decimal[2]` se rechazaba al indexar porque `lowerRawElementAddress`
no obtenía layout de `builtin:decimal` desde `elementLayout`. La ruta de
arreglos administrados ya usa el layout decimal del destino. Se aplica esa
misma fuente de tamaño y alineación al acceso raw cuando falta el layout y
el elemento es decimal. Los demás tipos conservan su ruta anterior.

Base: `a56498f051939b8e19410212a5dd1585b1fef117`. La reproducción conservada
en `tests/integration/decimal_fixed_array` comprueba escritura y lectura de
ambos elementos y controles equivalentes de `int` y `double`.

La comparación usó un compilador experimental enlazado en A con el objeto
`MirNodeMachine.cpp` recompilado desde esta rama delante de copias de las
bibliotecas y de los dos objetos de entrada del build de la campaña. Los
headers, objetos y bibliotecas restantes provienen del árbol principal con
cambios locales: no es un build limpio de esta revisión. El log de enlace,
los comandos y hashes de bibliotecas están en
`A:/nsc-fixes-agente-2-evidencia/{baseline,fixed-array}/build.log`.

Antes del cambio, O0 y O2 fallaron con `C2.1 cannot determine raw element
layout for 'builtin:decimal'` (`baseline.log`). Después, ambos compilaron y
devolvieron 0 (`fixed-array/results.json`). El ejecutor CMake de la prueba
registrada también pasó (`fixed-array/cmake-fixed.json`). En este punto
stackalloc seguía fallando con CC1 en ambas optimizaciones; se investiga
separadamente. El compilador del árbol principal, que ya incluía el ajuste
sin registrar, pasó ambos casos (`reference.log`).

Las pruebas comprueban el caso concreto en Windows x64. Debe repetirse la
regresión con el compilador y SDK integrados de la rama principal; esta
evidencia híbrida no certifica el resto del compilador.
# Validación de integración posterior

La compilación incremental del compilador en la copia principal terminó con
código 0 y la prueba dirigida de arreglo fijo pasó; los comandos y resultados
figuran en `A:/nsc-fixes-evidencia/integrated-agent-tests-20261111.cmd` y
`.log`. El compilador incorpora otros cambios locales: no demuestra un árbol
limpio ni certifica los demás casos de decimal.

# NSC-NCSS-013: tamaño, alineación, arreglo dinámico y tipos inválidos (2026-11-14)

SCOPE LOCK. Objetivo: completar los requisitos de la ficha NSC-NCSS-013 que el
fixture histórico no cubre (tamaño, alineación, arreglo dinámico, aislamiento y
tipos inválidos). Naturaleza: correctiva y de pruebas. Base: `868c29e87` en
`development-fixes-limitaciones`. Modificables: `HirAggregateLowering.cpp`
(sólo la consulta `sizeof`/`alignof`), una prueba nueva y este documento.
Protegidos: `ProjectObjectLayoutModel::valueLayoutForTypeKey` y sus demás
consumidores, el fixture `tests/integration/decimal_fixed_array` y su runner,
las demás pruebas. Prohibido: merge, rebase, push, despliegue y cambios en
interoperabilidad C/C++. Terminación: la prueba nueva pasa en O0/O2 y las
pruebas existentes de `decimal`/`sizeof` no cambian de resultado.

Defecto: `sizeof(decimal)` y `alignof(decimal)` se rechazaban al bajar a HIR
con "sizeof cannot measure 'builtin:decimal': it has no value layout"
(compilador `6b8d051842d57126`, O0 y O2). `valueLayoutForTypeKey` no lista
`builtin:decimal`; los arreglos, `stackalloc` y el acceso raw ya tomaban el
layout `Decimal` del destino (16 bytes, alineación 8) como respaldo local. La
consulta de layout era un quinto consumidor sin ese respaldo.

Cambio: en `lowerAggregateOrLayoutExpression`, cuando el tipo medido es
exactamente `builtin:decimal` y el servicio compartido no da layout, se usa el
mismo `TargetDataLayout::WindowsX64().layoutOf(Decimal)`. No se modificó la
función compartida: los demás tipos y consumidores conservan su ruta. Queda
sin cubrir `sizeof(decimal[N])` (el servicio compartido sigue sin layout para
el elemento); no forma parte de la ficha y se registra como pendiente.

Prueba nueva `tests/integration/decimal_array_layout.py`: arreglo fijo y
dinámico con dos decimales distintos, reescritura de uno sin alterar el otro,
`Length`, paso entre elementos igual a `sizeof(decimal)`, alineación a
`alignof(decimal)` y conversiones válidas (`int` implícito, `double` explícito);
devuelve 118 (16 bytes, alineación 8) en O0 y O2. Cinco casos inválidos deben
fallar con su diagnóstico exacto: elemento `string`, índice `decimal`, lectura
a `int`, escritura `double` sin conversión y `string` en arreglo dinámico.

Resultados con `A:/nsc-fixes-evidencia/build/bin/nsc.exe` tras la compilación
incremental: la prueba nueva pasó. `ctest -R "decimal|sizeof|alignof"`: 18 de
20 pasaron; `nsc.cc1.correctness.cc1-c9-sizeof` (Core fuente rechazado por
coherencia de traits en `Arc.ns`/`WeakArc.ns`) y
`nsc.compiler.decimal-multiply-divide` (2 observaciones) fallan igual con el
archivo de HEAD recompilado, así que son previos y se registran aparte.
