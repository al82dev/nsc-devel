# SEM-03.01 - Snapshots densos de ownership y flujo

## Cuello de botella

Cada rama, loop, `try`, `switch`, expresión lógica y refinamiento de flujo copiaba un snapshot con
doce `unordered_set` y tres `unordered_map`. La mayoría de esas tablas sólo representaba ordinales
de locales, parámetros o storage nombrado del callable actual. Copiar buckets, nodos y hashes en
cada bifurcación era costo de asignación, no trabajo semántico.

## Representación nueva

- `localFlags`: un byte por local del callable, indexado por
  `globalLocalOrdinal - localBaseOrdinal`.
- `parameterFlags`: un byte por parámetro.
- `storageFacts`: vector contiguo con parámetros primero y locales después; conserva union activa,
  non-null, rango entero y case de variant.
- Los ordinales públicos permanecen globales. La densificación sólo existe dentro del snapshot.
- Se conservan dos sets de strings para proyecciones parcialmente movidas porque sus claves son
  paths estructurales (`L4/Header/Token`), no ordinales densificables.

Así se eliminan del snapshot todos los hash containers de ordinales y los tres mapas. Capture,
restore y join recorren slots contiguos y reconstruyen el estado mutable existente sin cambiar la
API pública.

## Reglas preservadas

- moved/maybe-moved, invoked/maybe-invoked e invalid/maybe-invalid usan el mismo join de tres
  estados.
- definite assignment sigue siendo intersección MUST.
- Un local creado después de un snapshot conserva assignment y union activa al restaurarlo.
- non-null se intersecta; rangos amplían min/max y conservan `excludesZero` sólo si ambas ramas lo
  prueban; variant/union sólo sobreviven si ambas ramas coinciden.
- Snapshots no alcanzables conservan el atajo anterior.

## Archivos nuevos

- `OwnershipFlowIntegerRange.h`
- `OwnershipFlowStorageFacts.h`
- `OwnershipFlowSnapshot.h`

Cada contrato nuevo vive en un archivo propio y está documentado en inglés dentro del código.

## Validación dirigida

- Compilar `nsc_project_method_body_model_tests` con warnings-as-errors.
- Ejecutar únicamente ese binario: 64 checks.
- Confirmar estáticamente que ningún consumidor usa los campos hash retirados.
- Sin baseline, CTest general, O1, O2 ni O3.
