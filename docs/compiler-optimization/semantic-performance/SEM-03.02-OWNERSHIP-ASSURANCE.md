# SEM-03.02 - Aseguramiento de snapshots densos

## Instrumentación

`ProjectOwnershipFlowStatistics` registra cantidades estructurales de capture, restore y join,
slots densos procesados y paths parciales copiados. El contador
`legacyOrdinalHashEntriesCopied` permanece en cero por contrato.

No se registran tiempos dentro de la semántica. Los contadores no se imprimen normalmente, no
entran en dumps y no afectan metadata ni hashes.

## Prueba dirigida

El fixture `DenseOwnershipSnapshots` ejecuta una bifurcación con un local y comprueba:

- que capture, restore y join fueron ejercidos;
- que hubo slots locales densos;
- que no se copiaron entradas hash de ordinales.

Esto evita una prueba que sólo compruebe compilación y no alcance el camino optimizado.

## Validación

- Compilar y ejecutar sólo `nsc_project_method_body_model_tests` en O0.
- Warnings-as-errors activos en el target.
- Sin baseline, CTest general, O1, O2 ni O3.
