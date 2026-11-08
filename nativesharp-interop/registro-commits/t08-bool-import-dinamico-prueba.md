# Tanda 8: prueba de importación dinámica bool

Se crean dos DLL sin runtime que exportan `Shared(_Bool)` con comportamientos opuestos. El fixture verifica atributos LLVM del wrapper y del reenvío contra Clang, y ejecuta ambas llamadas en O0–O3.

La inspección del IR completo pasó, pero la primera compilación ejecutable falló con `runtime-bound import lost its declaration`: en la unidad global, la restricción de declaraciones omite el wrapper cuya definición se intenta generar. Evidencia: `A:\nsc-interop-build\tanda08-bool-dynamic-16177-28170`. La corrección se registra aparte; ninguna ejecución O0–O3 queda acreditada por esta línea base.
