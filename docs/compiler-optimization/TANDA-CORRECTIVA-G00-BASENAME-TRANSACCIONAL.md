# Tanda correctiva G00 — basename estable en publicación transaccional

Fecha: 2026-10-01
Estado: terminada, pendiente de baseline completo

## Fallo observado

`ArtifactTransaction` aislaba cada salida agregando un prefijo ordinal al nombre del archivo
staged: `0-Producto.dll`, `0-Producto.exe` y `0-Producto.pdb`. Después del enlace renombraba los
archivos a su nombre público, pero el contenido del PE ya había congelado el basename staged.

Esto produjo varias cascadas aparentemente distintas:

- import libraries que declaraban `0-MatrixProducer.dll` en lugar de `MatrixProducer.dll`;
- registros CodeView RSDS que nombraban `0-DebugExtremeConsumer.pdb`;
- hosts C/C++ incapaces de comprobar el producto DLL esperado;
- verificación de runtime/import-library fallida después de un enlace correcto.

## Regla implementada

- Cada entrada se aísla en un subdirectorio ordinal propio dentro del staging.
- El archivo staged conserva exactamente el basename del destino público.
- Los múltiples productos de una transacción siguen sin colisionar.
- Commit, rollback, rutas largas y clasificación de errores mantienen su comportamiento.

## Archivos modificados

- `src/Basic/ArtifactTransaction.cpp`: usa `staging/<ordinal>/<basename>` y crea el subdirectorio.
- `tests/unit/NativeSharp3T34LongPathPublicationTests.cpp`: congela basename estable y aislamiento
  por directorio.

## Mejora obtenida

Los nombres internos grabados por LLD coinciden con los artefactos finalmente publicados. La
transacción sigue siendo atómica sin corromper contratos observables del formato PE.

## Validación específica

- Build exclusivo de `nsc`, T34, matriz DLL, hosts C/C++ e incremental-link: correcto.
- T34 publicación/rollback: 8/8.
- Matriz DLL LLD/NSLink, O0/O2: 108/108.
- Host C: 55/55.
- Host C++: 57/57.
- Extreme debug O0: cierre, PE, PDB, RSDS, unwind y corpus CodeView correctos.
- Debug CodeView/PDB O0/O2: correcto.
- Incremental-link avanzó hasta un defecto independiente: el staging no siembra el PE/sidecar
  anterior y por ello no puede producir hit; se reserva para otra tanda.
- No se ejecutó CTest ni baseline.
