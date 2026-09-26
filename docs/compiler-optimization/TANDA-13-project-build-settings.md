# Tanda 13: configuración de compilación en el proyecto

## Objetivo

Evitar que cada invocación tenga que repetir el nivel de optimización, el artefacto de salida, la raíz de bibliotecas y el número de workers cuando esos valores pertenecen al proyecto.

## Cambios realizados

- El formato `.nsproj` acepta cuatro propiedades nuevas dentro de `PropertyGroup`:
  - `OptimizationLevel`: `O0`, `O1`, `O2` u `O3`.
  - `OutputPath`: ruta completa del artefacto que se va a producir.
  - `LibraryReferenceRoot`: raíz usada para resolver `LibraryReference` relativos.
  - `CompilerJobs`: concurrencia entre 1 y 1024.
- Las rutas relativas se resuelven desde el directorio del `.nsproj`, no desde el directorio desde el que se ejecutó `nsc`.
- Las opciones escritas explícitamente en la línea de comandos conservan la máxima prioridad.
- El driver directo puede emitir OBJ, EXE, LIB o DLL con solamente la acción y el proyecto si existe `OutputPath`.
- Cuando `OutputPath` viene del proyecto, el driver crea de forma segura su directorio padre si aún no existe.
- `nsc build` consume los mismos defaults, acepta ahora `-j` como override y conserva como fallback su configuración anterior.
- La lectura de estos defaults sólo analiza el XML raíz; no enumera fuentes ni carga el grafo para decidir las opciones iniciales.
- El cargador normal también respeta `LibraryReferenceRoot` del proyecto cuando no se proporcionó uno externamente.

Ejemplo:

```xml
<PropertyGroup>
  <OptimizationLevel>O0</OptimizationLevel>
  <OutputPath>../build/O0/NativeSharp.Math.LinearAlgebra.lib</OutputPath>
  <LibraryReferenceRoot>../build/O0</LibraryReferenceRoot>
  <CompilerJobs>12</CompilerJobs>
</PropertyGroup>
```

Con esas propiedades, la invocación larga se reduce a:

```text
nsc --emit-project-oop-lib NativeSharp.Math.LinearAlgebra.nsproj
```

## Archivos modificados

- `include/nativesharp/Compilation/ProjectLoader.h`
- `src/Compilation/ProjectLoader.cpp`
- `src/Driver/Driver.cpp`
- `src/Driver/ProjectCommands.cpp`
- `tests/unit/ProjectLoaderTests.cpp`

## Archivos nuevos

- `docs/compiler-optimization/TANDA-13-project-build-settings.md`: este informe. No se agregaron nuevos archivos de implementación; la funcionalidad se integró en el cargador y los drivers existentes.

## Por qué se hizo así

El driver necesita conocer jobs y raíz de referencias antes de cargar el grafo, y necesita conocer optimización y salida antes de despachar el backend. Por eso existe una lectura ligera y validada de las propiedades de build. Reutilizar la carga completa en ese punto habría vuelto a enumerar fuentes y habría reintroducido trabajo duplicado en el camino crítico.

La precedencia queda definida como `CLI explícita > .nsproj > defaults anteriores`. Esto permite una configuración reproducible sin impedir compilaciones puntuales con valores diferentes.

## Mejora esperada

- Órdenes de compilación más pequeñas, reproducibles y menos propensas a mezclar una raíz O0 con una salida O2 o un número distinto de workers.
- Menos lógica duplicada en scripts externos y en generadores de comandos.
- Costo de configuración mínimo: se lee el XML, pero no se recorre el árbol de fuentes ni se construye el grafo para obtener estos valores.

## Verificación de la tanda

- Construcción de `nsc` en Debug/O0: correcta.
- Construcción del target dirigido `nsc_project_loader_tests`: correcta.
- `ctest -R '^nsc\\.project-loader$'`: 1/1 correcto en 0.22 segundos.
- Compilación O0 mínima sin `-O`, `-o` ni `-j`: produjo `Configured.lib`, su header y su NSMETA en el `OutputPath` configurado.
- Segunda compilación O0 con `-O0 -j1 -o Override.lib`: produjo el artefacto alternativo, confirmando la prioridad de CLI.
- `nsc build` sobre el mismo fixture, con `--no-restore` y O0: correcto y publicó en el `OutputPath` del proyecto.
- No se ejecutó baseline completo.
