# SEM-05.06 — Merge tipado y serial de resultados callable

## Propósito

Publicar resultados aislados sin que un shard escriba directamente sobre el modelo global y sin
confundir dominios de ordinales.

## Bases de remapeo

Cada resultado se fusiona con cinco bases independientes:

- `scopeBase`;
- `localBase`;
- `cleanupBase`;
- `lambdaBase`;
- `localBorrowBase`.

El remapper conserva ordinales globales de callable, miembro, parámetro, tipo y función. También
conserva `ProjectConstructionSite::sequenceWithinCallable`.

## Outputs

Se agregan scopes, locales y todos los bindings publicables de expresiones, construcción,
control de flujo, cleanup, ownership, préstamos, lambdas, delegates, eventos, carriers,
llamadas y funciones libres. Los nombres léxicos se conservan porque el cierre post-merge de
escape todavía los consulta; sólo los nombres locales reciben `localBase`.

## Índices

Los índices O(1) y mapas `BySyntax` se actualizan exclusivamente para el sufijo agregado. No se
reescanea el modelo completo después de cada callable. La regla newest-binding-wins conserva el
orden canónico de fusión.

## Diagnósticos y estadísticas

Los diagnósticos del shard se publican a través del presupuesto global. Los contadores de
ownership se suman; la profundidad máxima de continuaciones de escape se reduce con `max`.

## Integración

El coordinador serial ya ejecuta todos los callables mediante:

```text
contexto inmutable -> shard aislado -> resultado -> merge tipado
```

La política de oleadas y descubrimiento todavía es serial. Tanda 6 cambiará solamente el paso de
ejecución, no el contrato de resultado ni el merge.

## Validación dirigida

Build y ejecución individual de las pruebas de method bodies y escape unificado en O0. No se
ejecuta baseline, minibaseline, CTest general ni O1/O2/O3.
