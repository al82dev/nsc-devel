# T09: matriz de bases de enum class C++ variádico cdecl

## Alcance

La corrección anterior se contrastó para bases con y sin signo de 8, 16, 32 y 64 bits, en una función C++ cdecl variádica directa y un callback. Los casos prueban valores representativos, incluido el máximo `unsigned long long`; la función receptora lee el enum con `va_arg` y comprueba el argumento posterior. No se cambió el compilador en esta verificación.

## Evidencia

- Matriz completa: 16 combinaciones de base y ruta. Cada una ejecutó ocho comparaciones C++/NativeSharp en estática y DLL O0–O3, verificó el ancho en el IR de NSC y ejecutó 16 consumidores precompilados con y sin encabezado. `A:/temp/nsc-t09-cpp-enum-scoped-bases-xzizmz_l`.
- La misma matriz comprobó por separado el ancho en el IR O0 de Clang; los registros `control-clang-ir.log` están en cada carpeta de caso.
- La prueba reproducible `validar-bases.py` incorpora esa comparación de Clang. Se verificó el script actualizado en la ruta directa y por callback con base `signed char`: `A:/temp/nsc-t09-cpp-enum-scoped-bases-us4b_aqb` y `A:/temp/nsc-t09-cpp-enum-scoped-bases-y42s0zwb`. Admite seleccionar `--base` y `--ruta`.

## Límites

Las muestras cubren el destino Windows x64 y esta forma de firma; no prueban todos los valores ni otras convenciones o plataformas. La comparación del IR de Clang se hizo en O0; los consumidores precompilados se verificaron mediante ejecución, no por inspección de su IR. Los ítems generales T09-003, T10-002 y T10-003 siguen abiertos.
