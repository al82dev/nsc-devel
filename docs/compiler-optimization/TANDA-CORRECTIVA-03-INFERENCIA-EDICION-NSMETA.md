# Tanda correctiva 03: inferencia de edicion desde NSMETA

> **Vigencia:** NativeSharp es un lenguaje único y acumulativo; el producto es 1.0. Las referencias históricas a ediciones o versiones de lenguaje y NSMeta que aparezcan a continuación no definen compatibilidad ni deben reintroducirse.

## SCOPE LOCK histórico

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
  predeterminado de un propietario implicito antes de considerar la
edicion publicada por sus descriptores NSMETA. El mismo dato conservado por
`Project` no participaba en esta segunda ampliacion del grafo.

## Cambio histórico

La corrección de esa campaña calculaba una autoridad unica a partir de:

1. las ediciones declaradas expresamente por proyectos fuente;
2. las ediciones publicadas por los descriptores NSMETA unicos.

Los proyectos fuente implicitos adoptaban esa autoridad sin perder su procedencia.
El mecanismo rechazaba etiquetas explícitas incompatibles o descriptores con
etiquetas diferentes mediante `NS3001`.

**Contrato vigente:** el lenguaje es único. El grafo no infiere ni compara
versiones de lenguaje o NSMeta; valida contenido y contratos reales.

El cálculo de entonces era iterativo y lineal respecto de proyectos y descriptores. No agregó
recursion, estado global, trabajo por cuerpo ni costo en las rutas semanticas
calientes.

## Archivos

- `src/Compilation/ProjectGraph.cpp`: reconciliacion del cierre fuente/NSMETA.
- `tests/unit/PrecompiledLegacyLanguageLabelTests.cpp`: cobertura nueva de
  adopcion implicita y conflictos explicitos.
- `CMakeLists.txt`: registro de la prueba dirigida independiente.

## Criterio histórico de cierre

- La campaña comprobó la adopción implícita de la etiqueta publicada por una
  dependencia y el rechazo de etiquetas incompatibles. Esas reglas fueron retiradas.
- También comprobó la inferencia fuente y la constitución T00 de aquella revisión.

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
