# Tanda 8: puntero a bool C que sale de la importación

Un puntero `_Bool*` devuelto por C y otro guardado en una global C se usaron desde NativeSharp para leer y escribir dos elementos. La ejecución cruzada pasó O0–O3: `A:\nsc-interop-build\tanda08-bool-pointer-17140-20454`.

El IR muestra una diferencia: las funciones C `c_bool_probe` y `c_bool_write` emitidas por Clang acceden con `load i8` y `store i8`, mientras los accesos correspondientes de NSC emiten `load i1` y `store i1`. La prueba funcional usa valores C válidos de cero y uno y no demuestra equivalencia completa bajo optimización ni para calificadores, alias o punteros escapados.

`verificar_ir.py` es un diagnóstico separado que actualmente falla ante los patrones observados; no se añade como gate aprobado. El puntero devuelto se guarda en un local NativeSharp `bool*`: la procedencia C debe sobrevivir ese flujo antes de seleccionar la representación sin cambiar silenciosamente la de todos los punteros nativos. Faltan diseño, corrección y pruebas de la propagación de procedencia; T08-014 y T08-006 permanecen abiertos.
