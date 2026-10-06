# SortedList indexer compatible con el NSC oficial

## Problema

El getter `SortedList<TKey, TValue>.this[TKey]` publicaba `get throws KeyNotFoundException`. El NSC oficial
identificado por el usuario con build hash `eb9582e921fd9757` no admite contratos `throws` en accessors.

## Cambio

- El getter ya no declara `throws`.
- Una clave ausente devuelve `default(TValue)`, siguiendo el contrato total de los accessors de Core.
- Una clave presente continúa devolviendo el valor de su posición ordenada.
- `TryGetValue` conserva la forma no ambigua de distinguir ausencia y un valor default almacenado.
- `SetValue` mantiene el contrato estricto `throws KeyNotFoundException`.

## Compatibilidad

No se modificaron `LowerBound`, comparación, orden, ownership, inserción, eliminación ni crecimiento. El único
cambio observable es el resultado total del indexador para una clave ausente.

## Verificación

- Compilador local oficial: `C:\native-sharp\bin\nsc.exe`, build hash `2a44eedad51a7a9b`.
- NSLink: `A:\nslink-devel\build-release\bin\nslink.exe`.
- Configuración: O0 y referencias binarias en `A:\nativesharp-libraries\build\O0`.
- Resultado: exit code 0; emisión correcta de `.lib`, `.h` y `.nsmeta`.
- No se ejecuta baseline ni O1/O2/O3.
