# Dictionary indexer compatible con el NSC oficial

## Problema

El getter `Dictionary<TKey, TValue>.this[TKey]` publicaba `get throws KeyNotFoundException`. El NSC oficial
identificado por el usuario con build hash `eb9582e921fd9757` no admite contratos `throws` en accessors.

## Cambio

- El getter ya no declara `throws`.
- Una clave ausente devuelve `default(TValue)`, igual que los accessors totales de Core.
- Una clave presente continúa devolviendo el valor almacenado.
- `TryGetValue` conserva la forma no ambigua de diferenciar una clave ausente de un valor default almacenado.
- `SetValue` conserva su contrato estricto `throws KeyNotFoundException`; sólo cambió el indexador.

## Compatibilidad

No se cambió el layout, ownership, hashing, crecimiento ni resolución de colisiones. El indexador puede ser
consumido por compiladores oficiales que no implementan accessor throws.

## Verificación

- Compilador local oficial: `C:\native-sharp\bin\nsc.exe`, build hash `2a44eedad51a7a9b`.
- NSLink: `A:\nslink-devel\build-release\bin\nslink.exe`.
- Configuración: O0, referencias binarias en `A:\nativesharp-libraries\build\O0`.
- Resultado: exit code 0; se emitieron `.lib`, `.h` y `.nsmeta` canónicos del componente.
- No se ejecutó baseline ni O1/O2/O3.
