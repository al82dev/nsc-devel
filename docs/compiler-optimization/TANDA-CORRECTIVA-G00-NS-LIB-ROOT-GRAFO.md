# Tanda correctiva G00 — `NS_LIB_ROOT` y precedencia fuente en toda la API

**Fecha:** 1 de octubre de 2026  
**Rama:** `development`  
**Alcance:** carga de proyectos del compilador NativeSharp 3.0; sin baseline ni cambios de librerías

## Motivo

Cinco pruebas de ejemplos empaquetados fallaban primero al buscar `NativeSharpCore.lib` junto al
proyecto `NativeSharp.Core.IO`. `nscbuilder.py` ya entregaba correctamente
`NS_LIB_ROOT=A:\native-sharp-3.0-baseline\lib`, pero sólo el Driver traducía esa variable a un
fallback de paquetes. `ProjectCompilation`, `ProjectGraphLoader` y los comandos semánticos directos
usaban `ProjectLoader` sin ese fallback.

Después de habilitarlo apareció la segunda mitad del defecto: el Driver sustituía una dependencia
binaria por el proyecto fuente homónimo después de leer NSMETA, pero las APIs semánticas directas
no ejecutaban esa preparación. El resultado era que `NativeSharpCoreIO` perdía la visibilidad de
`NativeSharpCore` y diagnosticaba seis usos de `StringBuilder` como tipo desconocido.

## Cambios

### Fallback central de paquetes

- `ProjectLoader` lee `NS_LIB_ROOT` mediante `_dupenv_s` cuando no existe un root explícito ni un
  fallback entregado por el llamador.
- La variable conserva semántica de fallback: un artefacto válido junto al `.nsproj` gana; sólo un
  artefacto local ausente se busca en el staging.
- `LibraryReferenceRoot` del proyecto o de la línea de comandos conserva mayor precedencia.
- La lectura queda en el loader compartido, por lo que Driver, comandos semánticos, APIs embebidas
  y pruebas reciben el mismo comportamiento.

### Precedencia fuente para clientes directos

- `ProjectGraphLoader` detecta el caso no ambiguo donde los stems de `.lib` y `.nsmeta` coinciden
  exactamente con el nombre de un módulo fuente ya descubierto.
- En ese caso elimina sólo la arista binaria sombreada y agrega la arista de `ProjectReference` en
  memoria. Después vuelve a ordenar el grafo topológicamente mediante su recorrido normal.
- Si los nombres no coinciden exactamente, el loader no adivina: conserva la referencia binaria
  para que el Driver resuelva la identidad autoritativa desde NSMETA.
- La regla no modifica ningún `.nsproj`, no copia de C: hacia A: y no convierte `NS_LIB_ROOT` en un
  root obligatorio.

### Diagnóstico de los probes

- El probe Stage 53.3 incluye ahora los mensajes semánticos en su fallo. Esto permitió aislar
  `StringBuilder` y evita que una futura regresión vuelva a aparecer únicamente como “ownership
  analysis failed”.

## Archivos modificados

- `src/Compilation/ProjectLoader.cpp`
- `src/Compilation/ProjectGraphLoader.cpp`
- `tests/unit/ProjectLoaderTests.cpp`
- `tests/unit/ProjectGraphTests.cpp`
- `tests/unit/Stage53_3FilesystemPathsTests.cpp`
- `docs/compiler-optimization/TANDA-CORRECTIVA-G00-NS-LIB-ROOT-GRAFO.md` — expediente nuevo de
  causa, diseño, archivos, mejora y evidencia de esta tanda.

No se creó ningún directorio fuente nuevo ni se modificaron los proyectos de Core, Core.IO o
Core.Threading.

## Validación dirigida

- `nsc` construido correctamente.
- ProjectGraph: **35/35**.
- ProjectLoader: **87/87**.
- Stage 53.3 filesystem/paths: **39/39**.
- Stage 53.3b file content: **38/38**.
- Stage 53.4 filesystem closure: **37/37**.
- Stage 53.6 directory enumeration: **39/39**.
- Pre-Stage-56 utilities: **23/23**.
- Comando directo `--check-project-method-bodies` sobre Stage 53.3 con el `NS_LIB_ROOT` de staging:
  correcto, **1104 scopes, 472 locals, 4307 name bindings y 644 calls**.

Total: **298 comprobaciones dirigidas correctas**, además del comando semántico directo. Los dos
directorios temporales creados dentro de `A:\nsc-devel\build-test-lldlinker` fueron eliminados.
No se ejecutó baseline, CTest general, compilación externa de libraries, instalación, deploy ni
operación alguna sobre `C:\native-sharp`.

## Mejora concreta

`NS_LIB_ROOT` deja de ser una característica accidental de una sola ruta del Driver. Un proyecto
con dependencias fuente y binarias obtiene la misma resolución al compilar, consultar semántica o
ejecutarse dentro de una prueba. Esto elimina falsos “missing library” y evita que reemplazar una
dependencia binaria por su fuente rompa la visibilidad transitiva entre módulos.
