# T08: validación de global C atómico int no TLS

La construcción incremental del objetivo `nsc` terminó con código 0 en
`A:/nsc-interop-build/tanda1-ON`. La construcción mostró dos advertencias de
campos de inicializador ausentes en `CppSemanticInventory.cpp`; no se evaluó
su origen en este control.

`pruebas-tanda-08-atomic-global/validar.cmd` pasó O0–O3 y contrastó en el IR de
Clang y NSC una lectura atómica `seq_cst` y `atomicrmw add` sobre el mismo
global. El resultado fue observado también mediante la función C. La regresión
existente `pruebas-tanda-08-tls-c/atomic/validar.cmd` terminó con código 0.
El caso nuevo se agregó al gate agrupado, que no se ha repetido aún.

El resultado cubre `_Atomic(int)` global no TLS, no los demás tipos atómicos,
formas de publicación o toda la concurrencia. T08-004 y la tanda 8 siguen
abiertas; no se modificó NSLink.
