# T09-003: atributos de alineación en el replay precompilado de records C

## Defecto

Sondas `build-claude-t08/tools/sonda-empaquetado.py` y `sonda-directa.py`
(`A:/temp/nsc-t09-sonda-empaquetado-cxplwn7o`, `A:/temp/nsc-t09-sonda-directa-z2w4dvv8`). Con
plantilla precompilada y sin encabezado, un record C con un campo `_Alignas(16) int` se rechazaba:

- en los punteros `sysv_abi` y `regcall`, con `<convención> function pointer signature is not yet
  supported`;
- en la llamada `sysv_abi` variádica, con `sysv_abi variadic argument type is not yet supported`.

Con encabezado, o en sesión viva, el mismo caso pasaba. Clang lo compila.

Los fixtures `pruebas-tanda-09-precompilado-campos-calificados`,
`pruebas-tanda-09-precompilado-variadica-anidado` y los 14 `validar-precompilado.py` de
`pruebas-tanda-09-sysv-indirecta-*` usaban este rechazo como control negativo.

Causa en el código:

- `cRecordHasPlainReplaySyntax` (`ClangSession.cpp`) rechazaba cualquier atributo del record o de
  sus campos (`hasAttrs()`). El record quedaba sin `plainReplaySyntax`, y la planificación
  precompilada lo rechazaba.
- El catálogo no guarda atributos de alineación. El replay (`makeSimpleCRecordAbiReplay`,
  `CppCRecordAbiReplay.h`) tampoco podía escribirlos, así que el layout reconstruido no coincidía
  con el del catálogo.

## Cambio

- `ClangSession.cpp`: `cRecordHasPlainReplaySyntax` acepta en el record y en sus campos atributos
  `AlignedAttr` no dependientes. Así llegan `_Alignas`, `__attribute__((aligned))` y
  `__declspec(align)`. Cualquier otro atributo deja el record fuera, como antes. También lo deja
  fuera la alineación en un record con campos de bits.
- `CppCRecordAbiReplay.h`: para un record no empaquetado, sin campos de bits y con alineación mayor
  que un byte, el replay reconstruye la alineación:
  - cada campo cuyo desplazamiento observado no es el natural recibe la menor alineación potencia de
    dos que da ese desplazamiento, como `__attribute__((aligned(N)))`;
  - si la alineación del record sigue siendo mayor que la que dan sus campos, el primer campo la
    recibe.

  Si ninguna alineación da el desplazamiento observado, el replay se rechaza.

  Se excluyen los records de alineación de un byte. El productor (`CppSemanticRecord.cpp`) consulta
  el replay de un record empaquetado antes de marcarlo como tal, y la inferencia rechazaría sus
  desplazamientos no naturales.
- Quien llama sigue comparando todo el layout reconstruido con el catálogo: tamaño, alineación,
  desplazamientos y tipos. Un record cuyo replay no coincide se rechaza igual que antes.

- `tests/unit/CppClangAbiProbeTests.cpp` (CTest `nsc.cpp-interop.clang-abi-probe`) exigía que un
  campo `aligned(8)` dejara el record fuera. Esa comprobación pasa a exigir lo contrario. Se añaden
  tres casos del productor:
  - un record con `aligned(16)`, que se acepta;
  - un campo de bits junto a un campo alineado, que se rechaza;
  - un campo `deprecated`, que se rechaza.

  También se añade un recorrido de ida y vuelta para un campo alineado, un record alineado y una
  unión con `_Alignas`. El replay lleva `aligned(`, el layout reconstruido coincide con el original
  y un desplazamiento que ninguna alineación explica no se reconstruye.

No se modificó Clang ni NSLink. El catálogo no cambia de forma y el contrato NSMeta vigente sigue
siendo el mismo.

## Evidencia

### Fixture nuevo

`pruebas-tanda-09-precompilado-alineacion/validar.py`, con y sin `--dll`.

Records C `T09R`:

| Caso | Declaración |
|---|---|
| `alignas-campo` | `{ int x; _Alignas(16) int z; }` |
| `alignas-char` | `{ char c; _Alignas(8) char d; int x; }` |
| `alignas-primero` | `{ _Alignas(16) int x; int y; }` |
| `alignas-final` | `{ int a; int b; _Alignas(8) int c; }` (solo cambia la alineación del record) |
| `alignas-double` | `{ int x; _Alignas(32) double d; }` |
| `aligned-record` | `struct __attribute__((aligned(16))) { int x; int y; }` |
| `declspec-record` | `struct __declspec(align(16)) { int x; int y; }` |
| `union-alignas` | `union { _Alignas(16) int x; long long y; }` |

Para `sysv_abi` y `regcall`, cada caso, de O0 a O3:

- se prueba con un puntero `T09Ptr (T09R r, int k)` devuelto por la biblioteca;
- un control C compilado por Clang 23.1.0 y el consumidor NativeSharp de la plantilla, con y sin
  encabezado, devuelven el mismo valor;
- la llamada indirecta del consumidor lleva la convención de Clang.

Además, para la llamada directa `sysv_abi` variádica `t09_v(T09R r, int k, ...)`, con el record
como argumento fijo y como argumento variable, los argumentos físicos del IR de NSC (tipo,
extensión y `byval`) coinciden con los de la llamada de Clang.

Hay dos controles negativos, rechazados en los dos punteros y en la llamada variádica:

- `{ int x; int y; __unaligned int z; }`: el replay no reconstruye `__unaligned`;
- `{ int x : 4; _Alignas(16) int z; }`: el productor deja fuera la alineación con campos de bits.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t09-alineacion-8ejn6_cr` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t09-alineacion-mmarauho` |

### Negativos obsoletos convertidos

Se aplica la política de negativos obsoletos. Cada rechazo `_Alignas` pasa a ser una comparación
positiva, y el rechazo se conserva con `__unaligned` en el mismo sitio y con la misma comprobación:

- `pruebas-tanda-09-precompilado-campos-calificados/validar.py`: el caso `alignas` se compara en
  punteros y en la variádica. El negativo es ahora `unaligned`.
- `pruebas-tanda-09-precompilado-variadica-anidado/validar.py`: el caso `alignas-anidado` (record
  anidado `T09InA` con `_Alignas(16)`) se compara. El negativo es ahora un record anidado `T09InU`
  con `__unaligned`.
- Los 14 `pruebas-tanda-09-sysv-indirecta-*/validar-precompilado.py` se convirtieron con
  `build-claude-t08/tools/transformar-alignas.py`:
  - cada `variante-alignas…` pasa a admitida y se ejecuta;
  - se añade una `variante-unaligned…` con el mismo campo declarado `__unaligned`, que debe dar
    `sysv_abi function pointer signature is not yet supported`.

| Fixture | Estática | DLL |
|---|---|---|
| campos calificados | `A:/temp/nsc-t09-campos-calificados-s_8cimpx` | `A:/temp/nsc-t09-campos-calificados-wp51jp03` |
| variádica anidado | `A:/temp/nsc-t09-variadica-anidado-vqsdbs7c` | `A:/temp/nsc-t09-variadica-anidado-tc50y_5m` |

Los 14 guiones `sysv-indirecta` pasan en la regresión de la tanda.

Regresión de la tanda: ver [TANDA-09-VALIDACION-AGRUPADA.md](../TANDA-09-VALIDACION-AGRUPADA.md).

## Límites

- `__unaligned` sigue fuera del replay: es un calificador que el catálogo no guarda.
- La alineación en records con campos de bits sigue fuera.
- Cualquier atributo distinto de `AlignedAttr` sigue fuera, así como `#pragma pack`, que Clang
  representa con otro atributo.
- La alineación reconstruida es la menor que da el layout observado. Puede diferir de la declarada,
  por ejemplo `_Alignas(8)` en un campo que ya cae en un múltiplo de 8. El layout resultante, que
  es lo que determina el plan ABI de Clang, es el mismo y se compara con el catálogo.
- Los records C++ no cambian: su productor sigue exigiendo registros sin atributos.
