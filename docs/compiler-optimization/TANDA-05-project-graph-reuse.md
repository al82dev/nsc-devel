# Tanda 5: reutilización del grafo de proyecto

## Objetivo

Evitar que una compilación cargue y expanda dos veces el mismo proyecto, sus referencias y sus patrones de archivos.

## Cambios realizados

- `ProjectCompilation` acepta un `ProjectGraph` precargado antes de iniciar el frontend.
- El driver transfiere al compilador el mismo grafo que ya cargó para configurar módulos precompilados.
- `compileSyntaxCore` carga el grafo solamente cuando no recibió uno previamente.
- Se añadieron validaciones para impedir que el grafo se sustituya después de comenzar la compilación.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectCompilation.h`
- `src/Frontend/ProjectCompilation.cpp`
- `src/Driver/Driver.cpp`

## Archivos nuevos

- Este informe. No se creó un caché paralelo ni un formato nuevo: se transfirió propiedad del grafo existente para mantener una sola fuente de verdad.

## Mejora esperada

Cada invocación del driver deja de repetir lectura XML, resolución de referencias, recorrido de directorios y expansión de globs antes del análisis sintáctico.

## Verificación de la tanda

- Construcción de `nsc` en configuración Debug/O0: correcta.
- Prueba dirigida `nsc.project-compilation`: correcta.
- Prueba dirigida productor/consumidor `nsc.item326.anonymous-member-promotion`: correcta.
- No se ejecutó baseline completo.
