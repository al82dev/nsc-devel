# SEM-06.08 — Handoff compacto del resumen de escape

## Defecto encontrado durante el cierre

El shard aislado reservaba un vector exterior de resúmenes con el tamaño de todos los callables,
pero dejaba vacía la entrada del callable analizado. Por ello `noteParameterEscape` descartaba las
clasificaciones `Return` y `Global`; el reducer tampoco tenía un resumen local que publicar. El
mismo diseño añadía inicialización O(N) a cada shard y, por tanto, O(N²) al conjunto de callables.

## Corrección

Cada shard conserva exclusivamente:

- el ordinal estable de su callable;
- una copia del resumen inicial de sus propios parámetros;
- las actualizaciones derivadas durante el análisis de ese cuerpo.

`ProjectCallableResultMerger` mueve ese único resumen a la ranura global antes del cierre
interprocedural por worklist. No hay acceso concurrente al modelo coordinador, no se recorre el
catálogo global y no se introduce recursión.

## Complejidad y ownership

- inicialización del shard: O(P) para los parámetros del callable, no O(N) callables;
- publicación: O(P) mediante movimiento de un vector de propiedad exclusiva;
- memoria transitoria por shard: O(P);
- el coordinador sigue siendo el único escritor del catálogo global.

## Archivos

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`;
- `src/Frontend/ProjectMethodBodyModel.cpp`;
- `src/Frontend/ProjectCallableResultMerger.cpp`.

## Validación dirigida

La evidencia roja previa fue `3 of 8 checks` en
`nsc_ns3_t06_escape_summary_tests`: se perdían los contratos de cuerpo, el round-trip NSMETA y el
rechazo de almacenamiento de stack entregado a un parámetro `Global`. El cierre exige reconstruir
y ejecutar únicamente ese objetivo, además de los tests dirigidos de method bodies y reducer. No
se ejecutan baseline, minibaseline, CTest general ni optimizaciones distintas de O0.

Resultado posterior:

- `nsc_ns3_t06_escape_summary_tests`: 8/8;
- `nsc_project_callable_wave_reducer_tests`: 5/5;
- `nsc_project_method_body_model_tests`: 76/76;
- build dirigido con warnings como errores: correcto;
- clang-tidy dirigido del merger: limpio;
- clang-tidy del TU monolítico `ProjectMethodBodyModel.cpp`: se interrumpió sin diagnóstico
  después de 150 segundos; no se presenta como aprobado. La compilación `/WX` y las pruebas
  anteriores sí cubren las líneas modificadas.
