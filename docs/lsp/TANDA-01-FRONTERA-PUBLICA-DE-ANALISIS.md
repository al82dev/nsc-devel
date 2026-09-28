# Tanda 1: frontera pública de análisis

## Objetivo

Crear el primer límite reutilizable entre el frontend de NSC y el futuro repositorio independiente
`A:\nsc-lsp`, sin copiar el parser ni exponer directamente todos los modelos internos del compilador.

## Diagnóstico

NSC ya separaba compilación, target y frontend en bibliotecas estáticas, pero sólo instalaba
`nsc.exe` y no publicaba un target pensado para consumidores. La identidad de build también vivía
dentro de `nsc_driver`, que arrastra backend, runtime y linker aunque un editor no los necesita.

## Cambios

- Se creó `nsc_basic` para compartir la identidad canónica sin enlazar el driver completo.
- Se creó `nsc_language_service`, una fachada estática con implementación oculta mediante PImpl.
- La primera operación pública entrega release, versión de lenguaje, stage, target Windows x64,
  hash de build y versión del ABI de análisis.
- CMake exporta `NativeSharp::LanguageService` y sus dependencias desde el árbol de build para que
  `A:\nsc-lsp` pueda enlazarlas estáticamente.
- Se agregó una prueba contractual exclusiva de identidad.

## Archivos modificados

- `CMakeLists.txt`: targets, dependencias, exportación y prueba específica.

## Archivos nuevos

- `include/nativesharp/LanguageService/LanguageService.h`: frontera pública estable.
- `src/LanguageService/LanguageService.cpp`: implementación privada inicial.
- `tests/unit/LanguageServiceIdentityTests.cpp`: contrato medido de identidad.
- `docs/lsp/TANDA-01-FRONTERA-PUBLICA-DE-ANALISIS.md`: memoria de la tanda.

## Mejora obtenida

El servidor de lenguaje podrá ser otro repositorio y producir un solo ejecutable, pero seguirá
usando exactamente el frontend de NSC. La API pública no obliga al LSP a depender de AST, HIR, MIR,
LLVM o linker.

## Verificación

- Construcción exclusiva de `nsc_language_service_identity_tests`.
- Ejecución directa de ese binario.
- Construcción exclusiva de `nsc` para comprobar el nuevo enlace de la identidad compartida.
- Comprobación de generación de `NativeSharpLanguageServiceTargets.cmake`.
- No se ejecutó baseline ni CTest general.
