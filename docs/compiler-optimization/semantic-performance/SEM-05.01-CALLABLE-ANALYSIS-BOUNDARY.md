# SEM-05.01 — Frontera explícita de análisis por callable

## Propósito

Convertir el cuerpo inline del bucle de method bodies en una unidad semántica explícita que
pueda recibir contexto propio y producir un resultado aislado en los siguientes commits.

## Cambio

El bucle de oleadas invoca ahora `analyzeCallable`. La unidad separa:

1. admisión y constraints de la especialización;
2. reset del scratch perteneciente al callable;
3. creación de scope raíz y parámetros;
4. cleanups de parámetros owned o con drop;
5. argumentos de `this(...)`/`base(...)`;
6. análisis del bloque;
7. continuaciones y cierre de cleanups.

La extracción conserva el orden y usa los mismos vectores, ordinales y diagnósticos. Todavía no
introduce threads: su finalidad es fijar una frontera comprobable antes de mover estado.

## Archivos

- `ProjectMethodBodyModel.h`: operaciones privadas de la unidad callable;
- `ProjectMethodBodyModel.cpp`: implementación y simplificación del bucle de oleadas;
- este documento.

## Mejora

- elimina más de cien líneas de lógica embebida en `build`;
- hace visible todo el lifecycle de un callable;
- permite introducir contexto/resultados locales sin duplicar la lógica semántica;
- no cambia la política de descubrimiento de especializaciones.

## Validación dirigida

Build y ejecución individual de las pruebas de method bodies y de escape unificado en O0. No se
ejecuta baseline, minibaseline, CTest general ni O1/O2/O3.
