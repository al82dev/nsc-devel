# Tanda 5: índice semántico básico

## Objetivo

Publicar símbolos de documento y workspace, hover inicial e ir a definición usando exclusivamente
los bindings que ya resolvió el frontend.

## Diagnóstico

Los modelos de nombres, miembros y cuerpos ya conservaban declaraciones, rangos y selecciones de
overload, pero no existía una vista compacta para herramientas. Recorrer texto o reconstruir reglas
en el LSP produciría respuestas distintas del compilador.

## Cambios

- El snapshot válido construye un índice de tipos, miembros, funciones libres, estado, parámetros y
  locales.
- Cada símbolo publica nombre, nombre calificado, detalle semántico, clase y ubicación UTF-16.
- Los bindings de tipos, nombres, llamadas y funciones libres enlazan usos con declaraciones.
- Las consultas de documento, workspace, símbolo en posición y definición operan sobre el snapshot
  inmutable.
- Un snapshot inválido reutiliza también el índice del último modelo válido.

## Archivos modificados

- `include/nativesharp/LanguageService/ProtocolTypes.h`.
- `include/nativesharp/LanguageService/LanguageService.h`.
- `src/LanguageService/LanguageService.cpp`.
- `CMakeLists.txt`.

## Archivos nuevos

- `tests/unit/LanguageServiceSymbolTests.cpp`.
- `docs/lsp/TANDA-05-INDICE-SEMANTICO-BASICO.md`.

## Mejora obtenida

El LSP podrá limitarse a traducir respuestas al protocolo. La selección de overload, visibilidad y
tipos proceden de la misma compilación que decide si el programa es válido.

## Verificación

- Construcción y ejecución exclusivas de `nsc_language_service_symbol_tests`.
- Símbolos de documento/workspace, hover semántico y definición de overload en Stage 27.
- Construcción exclusiva de `nsc`.
- No se ejecutó baseline ni CTest general.
