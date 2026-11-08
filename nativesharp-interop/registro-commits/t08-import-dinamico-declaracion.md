# Tanda 8: declaración de importaciones dinámicas en la unidad global

La unidad LLVM que posee los globales define los wrappers para símbolos importados que necesitan resolución dinámica. Con declaraciones restringidas, podía omitir la declaración del wrapper e interrumpir la compilación. Se conserva esa declaración específicamente en la unidad dueña cuando `importRequiresRuntimeBinding` está activo.

Tras la construcción incremental, dos DLL que exportan el mismo símbolo `Shared(_Bool)` compilaron y ejecutaron en O0–O3 con resultados diferentes y correctos. El IR completo coincidió con Clang en los atributos bool: `A:\nsc-interop-build\tanda08-bool-dynamic-16438-3306`. También pasaron las regresiones de callbacks C y llamadas C++ directas en O0–O3.

La prueba cubre este caso de importación dinámica, no acredita todas las firmas ni todas las rutas de compilación. La tanda sigue abierta.
