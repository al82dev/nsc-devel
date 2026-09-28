# Tanda 6: consultas semánticas avanzadas

## Objetivo

Completar la frontera de análisis que consumirá `nsc-lsp` con cinco operaciones de editor:
completion, signature help, referencias, rename y semantic tokens. Todas se calculan sobre el
modelo semántico certificado por el frontend; ninguna vuelve a interpretar NativeSharp con regex.

## Cambios

- `ProtocolTypes.h` incorpora DTO estables para completion, firmas, ediciones de texto, resultado
  de rename y tokens semánticos.
- `LanguageService.h` expone las cinco consultas desde un `AnalysisSnapshot` inmutable.
- `LanguageService.cpp` indexa el token léxico exacto de declaraciones y usos, conserva la
  sobrecarga elegida por resolución semántica y relaciona argumentos AST con parámetros.
- Completion filtra por prefijo y evita ofrecer locales o parámetros de otro documento.
- Referencias y rename agrupan una familia de sobrecargas con la misma identidad cualificada.
- Rename solamente devuelve ediciones: no escribe archivos. Rechaza keywords, identificadores
  inválidos y cualquier símbolo o referencia cuyo rango no sea un token inequívoco.
- Semantic tokens se generan exclusivamente para rangos exactos y distinguen declaración de uso.

## Archivos nuevos

- `tests/unit/LanguageServiceAdvancedQueryTests.cpp`: prueba focalizada sobre el proyecto Stage 27.
- Este documento de trazabilidad.

## Archivos modificados

- `include/NativeSharp/LanguageService/ProtocolTypes.h`
- `include/NativeSharp/LanguageService/LanguageService.h`
- `src/LanguageService/LanguageService.cpp`
- `CMakeLists.txt`

## Mejora obtenida

El futuro servidor LSP no necesita conocer clases internas del compilador ni inventar semántica.
Puede traducir directamente estos resultados al protocolo y aplicar rename sólo después de que el
editor confirme el workspace edit. Los rangos usan posiciones UTF-16, como exige LSP.

## Verificación permitida

Se construye y ejecuta únicamente `nsc_language_service_advanced_query_tests`, seguido de una
construcción O0 del target `nsc`. No se ejecuta baseline ni CTest general.
