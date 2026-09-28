# Tanda 24: idioma en comandos de proyecto

## Objetivo

Completar `--language es` en los comandos `new`, `restore`, `build`, `run`, `clean`, `add`, `remove` y `list`, sin traducir el protocolo JSON ni los diagnósticos técnicos del frontend.

## Diagnóstico

Los comandos de proyecto se despachan antes del parser histórico del compilador y concentran sus respuestas en una frontera propia. Traducir cada algoritmo habría duplicado lógica y podía introducir diferencias funcionales entre inglés y español.

## Cambios

- Se agregó una función central que traduce mensajes canónicos de proyecto al momento de presentarlos.
- Éxitos y fallos humanos pasan por esa frontera sólo en salida de texto.
- Se cubren creación, restauración, construcción, ejecución, limpieza, paquetes y validaciones comunes.
- `--output-format=json` conserva mensajes canónicos en inglés, claves, esquema y estructura, sin depender de `--language`.
- Los mensajes internos que no pertenecen al catálogo permanecen en inglés en vez de aplicar reemplazos ambiguos.

## Archivos modificados

- `include/nativesharp/Driver/CompilerConsole.h`: expone la traducción de mensajes humanos canónicos.
- `src/Driver/CompilerConsole.cpp`: catálogo compacto y transformaciones seguras por prefijo.
- `src/Driver/ProjectCommands.cpp`: localiza en la frontera de salida de texto.
- `tests/unit/ProjectCommandsTests.cpp`: verifica error español y neutralidad JSON.

## Archivo nuevo

- `docs/2.0/development/namespace-functions/TANDA-24-IDIOMA-COMANDOS-DE-PROYECTO.md`: memoria de implementación y verificación.

## Mejora obtenida

El mismo switch de idioma funciona tanto en las opciones históricas como en el flujo moderno de proyectos. La localización no interviene en resolución, compilación, restauración, archivos, caché, ABI ni protocolos consumidos por herramientas.

## Verificación

- Construcción incremental exclusiva del target `nsc`.
- Construcción y ejecución individual de `nsc_project_commands_tests`.
- Comprobación directa de un error de `new` en español y del mismo error en JSON neutral.
- No se ejecuta baseline ni CTest general.
