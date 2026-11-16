# T18-010 y T18-011: ICU4C 80 (`icuuc`) desde NativeSharp

## Comprobación

Este cambio comprueba una biblioteca C++ real; el código de producción es el del
[cambio 1](t18-constructores.md).

## Selección (T18-010)

ICU4C 80 (`A:/al82dev-clang-deps/icu/icu4c`), la biblioteca `common`: 203 unidades C++ y C de compilación
separada, namespace versionado (`icu_80`, con el alias `icu`), clases derivadas de `UObject` con destructor
virtual, memoria propia (`uprv_malloc`) y constructores sin `noexcept`. `icu::Edits` guarda su lista de
cambios en un arreglo que crece fuera del objeto y libera en el destructor.

## Evidencia (T18-011)

Fixture nuevo `pruebas-tanda-18-icu/validar.py`, con y sin `--dll`. Las 203 unidades se compilan con Clang
23.1.0 (`-std=c++17 -O2`, ocho procesos) en `icuuc.lib` o `icuuc80.dll`. El programa NativeSharp y un control
C++ usan `ParsePosition` (constructor con argumento, `getIndex`, `setIndex`, `setErrorIndex`), `Edits` (1000
cambios, `lengthDelta`, `numberOfChanges`, `hasChanges`; 200 objetos creados y destruidos con 600 cambios
cada uno) y `ErrorCode` (`isSuccess`). Resultado 1505555 en los dos, O0–O3.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática (`--crt-startup`) | pasa (O0–O3) | `A:/temp/nsc-t18-icu-nirgookq` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t18-icu-t0qraw01` |

## Límites

- `icu::UnicodeString` no importa sus constructores (`struct has only an implicit parameterless
  constructor`); sus métodos que devuelven `UnicodeString&` tampoco.
- El alias `icu` no sirve como prefijo de tipo en NativeSharp (`icu.UnicodeString x` no se analiza); se
  escribe `icu_80`.
- `UBool` es un entero: se compara con `0`.
