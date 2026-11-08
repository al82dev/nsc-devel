# Huella de condicionales de macros en encabezados C

La traza opcional del preprocesador registra los resultados de `#ifdef` y
`#ifndef`, y las variantes evaluadas o saltadas de `#elifdef` y `#elifndef`.
Estos condicionales pueden cambiar directivas como `#pragma pack` sin cambiar
los tokens visibles de las declaraciones. La prueba de sesión compara las
huellas de una cabecera con y sin la macro `FEATURE`. La construcción
incremental de `nsc_cpp_session_tests` terminó correctamente y la prueba de
sesión se ejecutó con código 0; el archivo existente de pruebas emitió avisos
de inicializadores incompletos durante la compilación.

Esta huella sigue siendo un componente experimental: por sí sola no acredita
la identidad de todas las entradas y no habilita la caché de referencias C/C++.
La validación integral de la tanda 8 permanece pendiente.
