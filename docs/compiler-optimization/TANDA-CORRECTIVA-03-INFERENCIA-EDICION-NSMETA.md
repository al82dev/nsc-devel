# Tanda correctiva 03: inferencia de edicion desde NSMETA

## SCOPE LOCK

- Se corrige exclusivamente la perdida de procedencia cuando un proyecto sin
  `<Language>` consume un cierre NSMETA con una unica edicion.
- No se modifican pruebas historicas, fixtures, proyectos, Core, runtime,
  metadata publicada ni consumidores.
- No se implementan features nuevos ni se relajan proyectos que declararon una
  edicion incompatible de forma explicita.
- La validacion es dirigida y O0; no se ejecutan baseline ni matrices generales.

## Causa raiz

La tanda 2 preservo la procedencia de la edicion y resolvio la inferencia entre
proyectos fuente. `ProjectGraph::addPrecompiledProjects` aun comparaba el valor
predeterminado Language 2 de un propietario implicito antes de considerar la
edicion publicada por sus descriptores NSMETA. El mismo dato conservado por
`Project` no participaba en esta segunda ampliacion del grafo.

## Cambio

`ProjectGraph` calcula una autoridad unica a partir de:

1. las ediciones declaradas expresamente por proyectos fuente;
2. las ediciones publicadas por los descriptores NSMETA unicos.

Los proyectos fuente implicitos adoptan esa autoridad sin perder su procedencia.
Una declaracion fuente explicita incompatible, dos descriptores incompatibles o
una misma metadata asignada a ediciones diferentes continúan produciendo
`NS3001`.

El calculo es iterativo y lineal respecto de proyectos y descriptores. No agrega
recursion, estado global, trabajo por cuerpo ni costo en las rutas semanticas
calientes.

## Archivos

- `src/Compilation/ProjectGraph.cpp`: reconciliacion del cierre fuente/NSMETA.
- `tests/unit/PrecompiledLanguageEditionInferenceTests.cpp`: cobertura nueva de
  adopcion implicita y conflictos explicitos.
- `CMakeLists.txt`: registro de la prueba dirigida independiente.

## Criterio de cierre

- Un propietario implicito adopta Language 3 desde un unico NSMETA Language 3.
- La procedencia del propietario permanece implicita.
- Language 2 explicito frente a NSMETA Language 3 sigue rechazado con `NS3001`.
- Dos autoridades NSMETA incompatibles siguen rechazadas con `NS3001`.
- Pasan la prueba nueva, la inferencia fuente de la tanda 2 y la constitucion T00.

## Evidencia dirigida

La compilacion con warnings como errores termino correctamente para:

- `nsc_precompiled_language_edition_inference_tests`;
- `nsc_language_edition_inference_tests`;
- `nsc_ns3_t00_constitution_tests`;
- `nsc_ns3_t33_package_coherence_tests`.

Pasaron completas las tres comprobaciones de contrato:

- `nsc.compilation.precompiled-language-edition-inference`;
- `nsc.compilation.language-edition-inference`;
- `nsc.ns3.t00.constitution`.

T33 paso cinco de seis checks: construyo Core O0, construyo Core.IO O0 contra
NSMETA, restauro el paquete y compilo el consumidor exclusivamente
precompilado. El ultimo check de ejecucion no es evaluable dentro de este
sandbox: el proceso anfitrion vacia `PATH` al iniciar `cmd.exe`, de modo que el
`cmd.exe` anidado y literal del fixture no puede resolverse. La comprobacion
directa `cmd.exe /c echo PATH=%PATH%` produjo `PATH=`. El fixture historico se
conservo intacto y el bloqueo no se presenta como aprobacion.

No se ejecuto baseline, minibaseline ni matriz O1/O2/O3.
