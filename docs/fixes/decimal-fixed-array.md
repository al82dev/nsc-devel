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
