# Punto compartido de trabajo en curso

Este punto conserva en la rama el trabajo local entrelazado para que pueda
clonarse y repartirse entre trabajadores. Es un **checkpoint WIP**, no una
declaración de que todos los cambios hayan terminado ni de que una revisión
limpia haya superado las pruebas. La base original de la campaña es
`152654e5a0e2d84d4c2f245ea7590488f0497430`; el commit previo al checkpoint
está disponible en el historial de la rama.

Se preservan en conjunto cambios de frontend, ownership, operadores,
metadata, HIR/MIR y limpieza porque varios comparten los mismos archivos y
dependencias. Los encabezados y pruebas `View*Prototype` son observaciones
experimentales: no acreditan un contrato binario verificado ni autorizan a
retirar la barrera de vistas. Algunos casos de async/finally, cancelación y
procedencia de vistas permanecen abiertos; consultar los registros concretos
antes de tratar notas históricas como estado actual.

El archivo `tests/unit/Freeze30ContractTests.cpp` se restauró a la revisión
previa por indicación expresa del usuario: su vaciado eliminaba 7,974 líneas
de pruebas funcionales. Se conservó el diferencial previo de los demás cambios
y 71 archivos nuevos con hashes en
`A:/nsc-fixes-evidencia/branch-cleanup-20261112`. La salida temporal `build`
se conservó en `A:/nsc-fixes-evidencia/untracked-build-search-20261112.log`.
Los cuatro fixtures que observaban resultados incorrectos de fallback decimal
se conservaron fuera de las regresiones en
`A:/nsc-fixes-evidencia/decimal-fallback-observations-20261112`.

Algunos ejecutores async y otras pruebas aún lanzan procesos hijos desde un
directorio temporal. El registro CMake por sí solo no corrige ese detalle: antes
de usarlos, ajustar su directorio de trabajo y confirmar TEMP/TMP/APPDATA y
LOCALAPPDATA en A desde el primer proceso. Mantener compilación incremental y
pruebas dirigidas; no ejecutar la batería completa. El compilador y Core de
las evidencias anteriores reutilizaron objetos o bibliotecas preexistentes.

El checkpoint no incluye interoperabilidad C/C++, Daikon ni fuentes de LLVM;
no modifica `A:/nsc-devel`, ni instala o modifica `C:/native-sharp`. Los
archivos compartidos con la rama de interoperabilidad requieren revisión al
integrar. `NativeSharp.Text` continúa aplazado por decisión del usuario.
