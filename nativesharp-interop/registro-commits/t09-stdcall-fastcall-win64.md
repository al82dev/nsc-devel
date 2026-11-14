# T09-003: declaraciones `__stdcall` y `__fastcall` en Windows x64

Se compararon dos fábricas C++ con un parámetro `int` y retornos `Probe*` y `Other*`, declaradas respectivamente `__stdcall` y `__fastcall`, en el destino `x86_64-pc-windows-msvc`. El control de Clang 23.1.0 y NativeSharp ejecutó cada fábrica y su método de instancia; un consumidor de plantilla precompilada repitió ambas llamadas con y sin encabezado.

Las muestras pasaron O0–O3 con biblioteca estática (`A:/temp/nsc-t09-instance-object-size-fub4wj5k`) y DLL (`A:/temp/nsc-t09-instance-object-size-y4ub5ix5`). El fixture comprueba con `llvm-nm` que los objetos Clang definen los símbolos decorados exactos y compara las llamadas del IR de Clang y NSC: para estas firmas y destino, ambas usan la convención LLVM predeterminada. Se compara el IR de control Clang O0 y el IR volcado de NSC; no se afirma equivalencia de todos los atributos ni del IR optimizado. Las ejecuciones verifican los valores resultantes y los controles anteriores también pasaron.

No se modificó producción. Esto no certifica x86 de 32 bits, otras firmas, llamadas indirectas ni todas las convenciones o símbolos C++. T09-003 y la tanda 9 siguen abiertos.
