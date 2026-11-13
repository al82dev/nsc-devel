# Registro C++ anidado pasado por copia en SysV

Se admite un nivel de registro interior trivial dentro de un argumento C++ pasado por copia. NSC coteja la estructura física observada por Clang y la disposición, identidad y elegibilidad del registro interior contra la sesión activa o contra el catálogo del consumidor precompilado. Una coincidencia de tamaño por sí sola no admite la firma; los registros interiores no pueden contener otro registro anidado en esta ruta.

El fixture `pruebas-tanda-09-sysv-cpp-copia-anidado` utiliza `Other { Inner {int,int}, double, int }` y retorno `{float,float,int}`. Tras la compilación incremental, pasaron las ocho comparaciones Clang 23.1.0/NativeSharp de bibliotecas estáticas y DLL en O0–O3 (`A:/temp/nsc-t09-sysv-cpp-copia-puntero-9odvzapz`) y los dieciséis consumidores precompilados con y sin encabezado (`A:/temp/nsc-t09-sysv-cpp-copia-puntero-precompilado-69vd42q0`). La prueba de plan ABI pasó controles de identidad ausente, offsets y anchuras incorrectos, y la comprobación dirigida de transporte e identidad terminó con código 0.

La ejecución acreditada cubre esa forma y destino `x86_64-pc-windows-msvc`; no certifica todas las formas de registros anidados, uniones, herencia, bitfields ni layouts empaquetados. T09-003 y la tanda 9 permanecen abiertos; se conservan D23 y D26.
