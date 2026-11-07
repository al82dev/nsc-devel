# T08: control de caché en proyecto TLS atómico

El caso `_Thread_local _Atomic(int)` importa `Api.h` por `CppLibraryReference`.
`PrecompiledModuleConfiguration.cpp` rechaza una caché de artefactos activa en
ese grafo porque todavía no rastrea encabezados transitivos. La prueba comprueba
el diagnóstico exacto antes de ejecutar el proyecto sin caché en O0–O3. No se
habilita ni se prueba reutilización de caché, y no se cambia NSLink.

Validación dirigida: `pruebas-tanda-08-tls-c/atomic/validar.cmd`, salida en
`A:/nsc-interop-build/tanda08-atomic-cache-guard.log`, código 0. Comprueba el
IR de Clang y NativeSharp, el rechazo de caché, y las ejecuciones positivas y
negativas del fixture. No sustituye la validación completa de T08.
