# Tanda de validación 03 — Identidad real de producto

## SCOPE LOCK local

- Defecto: `nsc_component_tests.exe` no enlaza `kCompilerVersion` ni
  `kCompilerBuildId`.
- Consumidores demostrados: `LanguageService.cpp` y `CompilerConsole.cpp`.
- Productor canónico: `src/Basic/Version.cpp`.
- Entrada generada necesaria: `nativesharp/Basic/BuildId.h` perteneciente al
  NSC seleccionado.
- Archivos funcionales autorizados: `tests/compiler-suite/CMakeLists.txt`.
- Producto, pruebas, manifiestos y seis archivos preexistentes: protegidos.

## Corrección

La suite compila el productor real y obtiene su include generado desde una de
dos fuentes deterministas:

1. `NSC_COMPILER_SUITE_BUILD_ID_HEADER`, cuando se proporciona explícitamente;
2. el árbol de construcción propietario del `nsc.exe` seleccionado.

La configuración falla con un diagnóstico preciso si el header no existe. No
se crea una identidad de prueba ni se copia un valor desde el ejecutable.

## Evidencia estática

- Símbolos faltantes de esta causa: `2/2` inventariados.
- Consumidores: `2/2` inventariados.
- Productor canónico: `1/1` incorporado.
- Cabecera generada local existente:
  `A:/nsc-devel/build-clang/generated/nativesharp/Basic/BuildId.h`.
- Compilación y enlace: pendientes de la siguiente corrida externa.
