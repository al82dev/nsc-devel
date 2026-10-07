# Tanda correctiva 11: lenguaje unico en proyectos

## Objetivo

Eliminar la edicion del lenguaje como frontera operacional entre proyectos fuente y modulos
precompilados, sin invalidar archivos `.nsproj` historicos.

## Cambios

- `<Language>` se acepta como entrada de compatibilidad, pero su valor ya no selecciona sintaxis.
- Todo `Project` publica una unica superficie acumulativa para parser, semantica y grafo.
- El grafo deja de rechazar referencias fuente o NSMETA por diferencias de edicion.
- La deteccion de ciclos, identidad de modulo, ownership de fuentes y validaciones de artefactos
  permanecen intactas.

## Archivos

- `include/nativesharp/Compilation/Project.h`
- `src/Compilation/Project.cpp`
- `src/Compilation/ProjectLoader.cpp`
- `src/Compilation/ProjectGraph.cpp`
- `src/Compilation/ProjectGraphLoader.cpp`

## Mejora

Un proyecto antiguo que el compilador todavia entiende puede participar en el mismo grafo que
codigo reciente. La compatibilidad depende de la sintaxis y del ABI realmente consumidos, no de
un numero administrativo en el proyecto o en NSMETA.

## Validacion

- Compilacion del objetivo `pruebas` antes del commit.
- La bateria completa se ejecutara despues de registrar este corte, conforme a la regla de no
  dejar cambios recuperables sin commit durante pruebas largas.
