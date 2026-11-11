# Stackalloc de decimal

`stackalloc decimal[2]` y la forma con cantidad calculada se rechazaban en
`lowerRawStorage` al no encontrar el layout de `builtin:decimal`. El ajuste
usa el tamaño y alineación decimal del destino únicamente para `StackAllocation`
de ese tipo si `elementLayout` no devolvió resultado. Los otros tipos y clases
de almacenamiento conservan la ruta anterior. El acceso a los elementos
necesita además el ajuste independiente de `decimal-fixed-array.md`.

La fuente de prueba proviene del trabajo local previo de la campaña. Incluye
un control `stackalloc double[2]`, un arreglo decimal de tamaño fijo y otro de
cantidad variable, con escritura y lectura de cada elemento.

La comparación usa un compilador experimental ensamblado en A: el objeto
`MirNodeMachine.cpp` procede de esta rama y los demás objetos, bibliotecas y
headers del build del árbol principal con cambios locales. No demuestra una
revisión limpia. El comando, enlace y hashes de las bibliotecas usadas están en
`A:/nsc-fixes-agente-2-evidencia/fixed-stackalloc/build.log`.

En la base, stackalloc falló en O0/O2 con `CC1 cannot determine the element
layout of 'builtin:decimal'` (`baseline.log`). Con solo la corrección de
acceso del arreglo fijo, seguía fallando (`fixed-array/results.json`). Con
ambas correcciones, los casos fijo y stackalloc compilaron y devolvieron 0 en
O0/O2 (`fixed-stackalloc/results.json`). Ambos ejecutores de regresión CMake
pasaron (`cmake-fixed.json`, `cmake-stackalloc.json`). El runtime usado fue
el aislado de la campaña en A; no se modificó la instalación de C:.

Los resultados cubren estas configuraciones de Windows x64 y deben repetirse
con el compilador y SDK integrados; no certifican otras configuraciones.
# Validación de integración posterior

La compilación incremental del compilador en la copia principal terminó con
código 0 y la prueba dirigida de stackalloc pasó; los comandos y resultados
figuran en `A:/nsc-fixes-evidencia/integrated-agent-tests-20261111.cmd` y
`.log`. El compilador incorpora otros cambios locales: no demuestra un árbol
limpio ni certifica los demás casos de decimal.
