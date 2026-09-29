# Tanda 8: acciones de código seguras

## Objetivo

Publicar acciones editables por LSP sin permitir que el servidor escriba archivos ni invente una
corrección cuando el frontend no puede demostrarla.

## Cambios

- LanguageService expone acciones con título, clase, preferencia, diagnósticos y TextEdit.
- UnexpectedCharacter permite ofrecer la eliminación del rango exacto rechazado por el lexer.
- El documento no formateado ofrece una acción SourceFixAll respaldada por el formateador.
- Los demás diagnósticos no producen quick fixes especulativos.
- Todos los rangos se validan contra el overlay actual usando posiciones UTF-16.

## Archivos

- include/nativesharp/LanguageService/ProtocolTypes.h
- include/nativesharp/LanguageService/LanguageService.h
- src/LanguageService/LanguageService.cpp
- tests/unit/LanguageServiceCodeActionTests.cpp
- CMakeLists.txt

## Mejora

nsc-lsp recibe acciones declarativas y VS Code decide si aplica el WorkspaceEdit. La frontera del
compilador mantiene la seguridad semántica y la extensión no interpreta mensajes humanos.

## Verificación permitida

Construcción y ejecución exclusiva de nsc_language_service_code_action_tests y construcción
incremental O0 del target nsc. No se ejecuta baseline ni CTest general.
