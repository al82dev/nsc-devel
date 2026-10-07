# Tanda correctiva 16: lenguaje acumulativo y metadata histórica

## Alcance bloqueado

- Ningún proyecto, fuente, dependencia o NSMETA puede rechazarse por declarar, omitir o mezclar
  una versión de lenguaje.
- La propiedad histórica `Language` se acepta únicamente como entrada administrativa ignorada.
- `unsafe` deja de ser una condición obligatoria para compilar; la sintaxis heredada continúa
  siendo reconocible para no romper fuentes existentes.
- Las pruebas que exigían rechazo por edición o contexto `unsafe` se convierten en pruebas de
  aceptación acumulativa.
- Los formatos NSMETA anteriores a 1.23 omiten el campo de edición y se proyectan a su
  representación histórica sin rechazar el modelo actual.
- Los ciclos reales del grafo siguen siendo errores estructurales y no dependen de una edición.

## Archivos de implementación

- `src/Compilation/ProjectGraph.cpp`
- `src/Interop/LibraryMetadata.cpp`
- Pruebas de proyecto, frontend, metadata y superficie que todavía codificaban rechazo por
  edición o por `unsafe`.

## Criterio de aceptación

1. No existe una ruta ejecutable que emita `NS3001` o `NS3002` por una versión declarada.
2. Las operaciones históricamente marcadas `unsafe` compilan sin exigir un bloque o modificador.
3. Los modelos actuales se pueden serializar en minors NSMETA históricos mediante proyección.
4. La batería completa O0 con NSLink termina sin fallos; una prueba dirigida no cierra la tarea.

## Estado

En implementación. La tarea permanece activa hasta obtener el reporte completo O0 limpio.
