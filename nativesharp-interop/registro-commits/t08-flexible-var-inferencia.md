# T08-010: inferencia de miembro flexible C

`var data = v->data` conservaba `builtin:int[*]` para un miembro flexible C y
el análisis semántico lo aceptaba, pero la generación LLVM rechazaba ese tipo
de local. `inferVariableType` ahora selecciona `T*` únicamente para un miembro
flexible importado cuya conversión ya se considera segura. Un miembro no apto,
como uno subalineado, se rechaza; los demás inicializadores mantienen su ruta.
El resultado original conserva los calificadores para el análisis posterior.

La compilación incremental y las pruebas O0–O3 de esta tanda quedan pendientes.
