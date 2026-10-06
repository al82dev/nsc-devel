# SortedDictionary indexer compatible con el NSC oficial

## Problema

El getter `SortedDictionary<TKey, TValue>.this[TKey]` publicaba `get throws KeyNotFoundException`. El NSC
oficial identificado por el usuario con build hash `eb9582e921fd9757` no admite contratos `throws` en
accessors.

## Cambio

- El getter ya no declara `throws`.
- Delega en el indexador total de `SortedList<TKey, TValue>`: una clave ausente devuelve
  `default(TValue)` y una presente devuelve su valor.
- `TryGetValue` sigue siendo la forma no ambigua de distinguir ausencia y un valor default almacenado.
- `SetValue` conserva su operación estricta con `throws KeyNotFoundException`.

## Compatibilidad

No cambió el ordenamiento, el comparer, ownership, crecimiento ni representación del contenedor. La API de
indexador deja de requerir una capacidad de accessor throws ausente en el compilador oficial indicado.

## Verificación

- Compilador local oficial: `C:\native-sharp\bin\nsc.exe`, build hash `2a44eedad51a7a9b`.
- NSLink: `A:\nslink-devel\build-release\bin\nslink.exe`.
- Configuración: O0 y referencias binarias en `A:\nativesharp-libraries\build\O0`.
- Resultado: exit code 0; emisión correcta de `.lib`, `.h` y `.nsmeta`.
- No se ejecuta baseline ni O1/O2/O3.
