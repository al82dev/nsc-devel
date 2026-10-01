# Tanda correctiva G00 — precedencia de proyectos fuente

Fecha: 2026-10-01
Estado: terminada, pendiente de baseline completo

## Fallo observado

Después de producir `NativeSharpCore.lib` y `NativeSharpCore.nsmeta` dentro del staging, las pruebas
self-host dejaron de fallar por artefacto ausente y expusieron un conflicto del grafo. El proyecto
raíz incorporaba `NativeSharpCore` como `ProjectReference`, mientras `NativeSharpCoreIO` lo
incorporaba como `LibraryReference`. El driver intentaba materializar ambas definiciones y detenía
la compilación con `duplicate module name 'NativeSharpCore'`.

## Regla implementada

- Un proyecto fuente ya presente en el grafo tiene precedencia sobre una referencia binaria al
  mismo nombre canónico de módulo.
- La sustitución ocurre sólo en el grafo en memoria. No modifica ningún `.nsproj`.
- El `LibraryReference` sombreado se retira del grafo efectivo para que no se materialice su
  NSMETA ni se enlace su archivo `.lib`.
- El proyecto que poseía la referencia binaria recibe una arista hacia el proyecto fuente. Así se
  conserva la visibilidad transitiva de tipos, genéricos y miembros.
- El grafo se vuelve a ordenar topológicamente y rechaza ciclos, proyectos desconocidos o mezcla
  de ediciones introducida por la sustitución.
- Si el módulo fuente no está presente, el comportamiento binario anterior permanece intacto:
  NSMETA, archivo COFF, cierre ABI y runtime siguen siendo obligatorios y validados.
- Dos productos precompilados incompatibles con el mismo nombre de módulo continúan siendo un
  error; esta corrección no debilita ese contrato.

## Archivos modificados

- `include/nativesharp/Compilation/ProjectGraph.h`: declara la sustitución fuente/binario.
- `src/Compilation/ProjectGraph.cpp`: reconecta las aristas, elimina el binario sombreado,
  restablece el orden topológico y valida el grafo resultante.
- `src/Driver/Driver.cpp`: detecta por identidad de módulo cuándo una referencia directa o de
  cierre binario está cubierta por un proyecto fuente.
- `tests/unit/ProjectGraphTests.cpp`: prueba que la referencia binaria desaparece, la arista fuente
  aparece y la raíz/orden del grafo se conservan.

## Mejora obtenida

El staging puede contener los artefactos bootstrap requeridos para compilar proyectos aislados sin
contaminar grafos que ya compilan esos módulos desde fuente. Esto elimina tanto la definición
duplicada como la pérdida de visibilidad que ocurriría si simplemente se omitiera el NSMETA.

## Validación específica

- Build exclusivo de `nsc_project_graph_tests`: correcto.
- `nsc_project_graph_tests`: 31/31 comprobaciones correctas.
- Build exclusivo de `nsc`: correcto.
- `LinkerAdapterSmoke.nsproj` emitido como PE O0 con el compilador recién construido: correcto,
  cero bibliotecas precompiladas enlazadas.
- `selfhost/nsc/NativeSharp.Compiler.nsproj` emitido como PE O0: correcto, 97 fuentes y cero
  bibliotecas precompiladas enlazadas.
- Los temporales de ambas emisiones fueron eliminados.
- No se ejecutó CTest ni baseline.
