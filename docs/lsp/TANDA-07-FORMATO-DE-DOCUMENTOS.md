# Tanda 7: formato conservador de documentos

## Objetivo

Dar al servidor de lenguaje una operación estable de formato sin duplicar reglas dentro de la
extensión de VS Code.

## Cambios

- LanguageService expone formatDocument con tamaño de tabulación y elección entre espacios o
  tabuladores.
- El formato conserva el tipo de salto de línea, literales y comentarios, elimina espacios al
  final y calcula sangría a partir de bloques NativeSharp.
- Si el texto ya está normalizado no se emite ninguna edición.
- La edición cubre el documento completo usando posiciones UTF-16 del contrato público.

## Archivos

- include/nativesharp/LanguageService/ProtocolTypes.h
- include/nativesharp/LanguageService/LanguageService.h
- src/LanguageService/LanguageService.cpp
- tests/unit/LanguageServiceFormattingTests.cpp
- CMakeLists.txt

## Mejora

nsc-lsp puede implementar textDocument/formatting usando el mismo servicio que mantiene los
overlays del editor. TypeScript sólo tendrá que convertir el TextEdit recibido.

## Verificación permitida

Construcción y ejecución exclusiva de nsc_language_service_formatting_tests, seguida de una
construcción O0 del target nsc. No se ejecuta baseline ni CTest general.
