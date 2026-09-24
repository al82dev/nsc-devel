# NativeSharp 2.0 — raíz configurable de `LibraryReference`

**Estado:** implementado  
**Branch:** `development`  
**Fecha:** 24 de septiembre de 2026  
**Alcance:** carga de proyectos; no modifica el formato `.nsproj`

## Problema

`LibraryReference` resolvía siempre `Include`, `Metadata` y `Runtime` contra el directorio que
contenía el `.nsproj`. Un proyecto que nombraba artefactos bajo `build\O0` no podía reutilizarse para
construir un grafo coherente O1, O2 u O3 sin reescribir el fichero.

Reescribir y restaurar proyectos durante el build dejaba el árbol expuesto a estados mixtos si el
proceso era interrumpido. Generar proyectos fuera del árbol tampoco era una alternativa general,
porque la enumeración de fuentes sigue estando arraigada en el directorio del proyecto.

## Contrato implementado

La opción global nueva es:

```text
--library-reference-root <absolute-directory>
--library-reference-root=<absolute-directory>
```

Cuando está presente:

- `LibraryReference Include` relativo se resuelve desde la raíz seleccionada;
- `Metadata` relativo se resuelve desde la misma raíz;
- `Runtime` relativo se resuelve desde la misma raíz;
- metadata implícita continúa derivándose del `.lib` ya redirigido;
- rutas absolutas conservan su significado;
- `Compile`, `ProjectReference` y `SdkReference` no cambian;
- sin la opción, el comportamiento previo permanece idéntico.

La raíz debe ser absoluta. La existencia de los artefactos se valida al cargar cada referencia, por
lo que el directorio no necesita existir antes de comenzar una compilación de dependencias que aún no
lo utiliza.

## Uso

El proyecto queda estable y declarativo:

```xml
<LibraryReference Include="NativeSharp.Foundation.lib"
                  Metadata="NativeSharp.Foundation.nsmeta" />
```

Cada pasada selecciona un conjunto completo:

```bat
nsc --emit-project-oop-lib NativeSharp.Collections.nsproj -O2 ^
    --library-reference-root A:\native-sharp-runtime-2.0\build\O2 ^
    -o A:\native-sharp-runtime-2.0\build\O2\NativeSharp.Collections.lib
```

Los comandos de alto nivel también transmiten la opción:

```bat
nsc build NativeSharp.Collections.nsproj -O2 ^
    --library-reference-root A:\native-sharp-runtime-2.0\build\O2
```

## Diseño

`ProjectLoadOptions` transporta la raíz sin estado global dentro de las capas de compilación. El
driver conserva las opciones únicamente durante una invocación y `ProjectGraphLoader` captura una
copia para que todos sus workers resuelvan referencias con el mismo contrato.

La opción se propaga por:

- `ProjectLoader`;
- `ProjectGraphLoader` y referencias transitivas;
- `ProjectCompilation`;
- carga auxiliar de metadata precompilada;
- comandos `check`/`dump` de proyecto y grafo;
- diagnostic bundles;
- comandos de alto nivel `build` y `run`.

## Archivos modificados

- `include/nativesharp/Compilation/ProjectLoader.h`
- `src/Compilation/ProjectLoader.cpp`
- `include/nativesharp/Compilation/ProjectGraphLoader.h`
- `src/Compilation/ProjectGraphLoader.cpp`
- `include/nativesharp/Frontend/ProjectCompilation.h`
- `src/Frontend/ProjectCompilation.cpp`
- `include/nativesharp/Driver/DiagnosticBundle.h`
- `src/Driver/DiagnosticBundle.cpp`
- `src/Driver/Driver.cpp`
- `src/Driver/ProjectCommands.cpp`
- `tests/unit/ProjectLoaderTests.cpp`
- `tests/unit/DriverTests.cpp`

## Validación dirigida

- `nsc_project_loader_tests`: `PASS: 73 checks`.
- `nsc_driver_tests`: `PASS: 222 checks`.
- `nsc_project_commands_tests`: `PASS: 14 checks`.
- `nsc.exe`: compilación correcta.

Los tests cubren comportamiento heredado, redirección conjunta de `.lib`/`.nsmeta`/`.dll`,
subdirectorios, selección O2 sin retener O0, ayuda, raíz relativa inválida, duplicados y propagación
real desde el driver.

No se ejecutó el baseline completo. La limitación independiente de fuentes fuera del directorio del
`.nsproj` permanece fuera de este cambio.
