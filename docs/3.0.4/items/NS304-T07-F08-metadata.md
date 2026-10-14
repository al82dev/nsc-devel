# NS304-T07-F08 — Metadata y debug

- **Estado**: IMPLEMENTADO (consumo precompilado `sin fuentes` + PDB de async frames)
- **Rama**: development

## Hueco demostrado y corregido

1. **Materialización del tipo opaco** (`Driver.cpp` `sourceTypeForMetadataKey`): `async-iterator:<T>|<E>`→`AsyncIterator<T,E>`, `iterator:<T>[|<E>]`→`Iterator<T[,E]>`, `future:<T>`→`Future<T>`. Sin esto el consumidor binario fallaba `cannot materialize type key`.
2. **Identidad física del constructor** (`LibraryMetadata.cpp`): el NSMETA publicaba `mangled=...Gen$drop` para el async-generator porque `functionForMember` devolvía el último `$drop` del split; ahora publica el **constructor** (`isGeneratorConstructor`), de modo que el consumidor llama a `Gen()` real y no al drop glue (corregía corrupción de heap en `Gen()`).
3. **No re-split del importado** (`ProjectOopHirModel.cpp`): el split async/generator se salta cuando `isImportedNative` (declaración desde NSMETA sin cuerpo), evitando re-sintetizar `$moveNext`/`$drop` duplicados.
4. **PDB**: con `-g` el PDB conserva los async frames de suspensión (`$moveNext$poll`/`$moveNext`/`$drop`) y la fuente.

## Pruebas

- `TL-LANG-470` — consumidor `sin fuentes` (`LibraryReference`, sin `.ns`) itera el `AsyncIterator<int,int>` importado → exit 6.
- `TL-LANG-471` — PDB no vacío con `$moveNext$poll`/`$moveNext`/`$drop` → exit 6.

## Commits

- `f9ee933f` — consumo `sin fuentes` del async-iterator.
- `fbcd1335` — PDB con async suspension frames.
