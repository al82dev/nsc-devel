# T19-030: replay precompilado sin encabezado de `__unaligned`, `#pragma pack`, otros atributos y bit-fields alineados

Una plantilla precompilada que se consume sin encabezado obtiene el plan ABI de Clang de un record C reconstruyéndolo
(*replay*). Después compara todo el layout reconstruido con el catálogo. Antes de este cambio quedaban fuera cuatro
formas; con encabezado ya pasaban.

## Defectos

Confirmados en el código (`ClangSession::cRecordHasPlainReplaySyntax`, `CppCRecordAbiReplay.h`) y con los negativos
de la tanda 9, que los rechazaban sin encabezado:

- `__unaligned`: `hasReplayBuiltinOrArray` solo admitía `const` y `volatile`. Clang no cambia por ese calificador ni
  el layout ni la clasificación del campo (`struct U { char c; __unaligned int x; double d; }` queda en 0/4/8).
- `#pragma pack(N)`: Clang lo representa con `MaxFieldAlignmentAttr`, y el productor solo admitía `AlignedAttr`.
- Atributos que no cambian ni el layout ni el paso, como `deprecated`, `unavailable`, `unused` y `annotate`: el
  productor los rechazaba igual que cualquier otro.
- Alineación en un record con bit-fields: el productor la dejaba fuera explícitamente, y el replay no la infería.

## Decisiones

- **`__unaligned`**: el productor lo admite, y el replay lo quita de la grafía al reescribir y al comparar el tipo de
  cada campo (`cppReplayWithoutUnaligned`). El layout y el tamaño se siguen comparando.
- **`#pragma pack(N)`**: el replay busca el menor N (1, 2, 4, 8 o 16) con el que cada campo, alineado a min(N, su
  alineación), cae en su desplazamiento observado, y el record queda con la alineación y el tamaño observados.
  Ese N se reescribe como `#pragma pack(push, N)`. Solo se hace en records sin bit-fields.
- **`#pragma pack(N)` con bit-fields**: el pack no se simula. Bajo `#pragma pack(N)` la alineación del record es
  min(N, la mayor alineación de campo), así que un record alineado por debajo de su mayor campo se reescribe bajo esa
  alineación. La comparación del layout decide.
- **`packed` en un campo**: un campo que cae antes de su alineación natural, en el primer byte libre, se reescribe con
  `__attribute__((packed))`.
- **Atributos sin efecto en el layout ni en el paso**: el productor admite `deprecated`, `unavailable`, `unused`,
  `annotate`, `may_alias`, `visibility`, `type_visibility` y `randomize_layout`, este último sin efecto sin semilla.
  - En el record admite además `ms_struct`, que es el layout por defecto de este destino, y `transparent_union`, que
    aquí no cambia el layout ni la clase de registro (Clang pasa la unión como `i64`).
  - En un campo admite `packed`.
  - `MaxFieldAlignmentAttr` solo se admite en el record.
  - `gcc_struct` no hace falta: Clang 23.1.0 lo rechaza en este destino («Itanium-compatible layout for the Microsoft C++
    ABI is not yet supported»), así que es un rechazo de Clang.
- **Alineación con bit-fields**: la inferencia de alineación recorre también los bit-fields. Cada corrida ocupa la
  unidad de almacenamiento de su tipo (layout de Microsoft). Si la alineación del record no la explica ningún campo y
  el primero es de bits, se escribe en el record (`__attribute__((aligned(N)))`).
- Como antes, nada se acepta sin comparar todo el layout reconstruido con el catálogo. Una inferencia que no coincide
  deja el record fuera con el diagnóstico de siempre, nunca con un plan distinto del de Clang.

## Qué funciona, como en Clang

En `pruebas-tanda-09-precompilado-alineacion`, para punteros `sysv_abi` y `regcall` y para la llamada directa
`sysv_abi` variádica (record como argumento fijo y como argumento variable, con los argumentos físicos del IR iguales a
los de Clang):

- `{ int x; int y; __unaligned int z; }`;
- `{ int x : 4; _Alignas(16) int z; }`;
- `#pragma pack(1)` `{ char c; int x; short s; }` y `#pragma pack(2)` `{ char c; int x; double d; }`;
- `#pragma pack(2)` con bit-fields: `{ char c; int a : 3; int b : 5; double d; }`;
- un campo `packed`: `{ char c; __attribute__((packed)) int x; short s; }`;
- un record `deprecated` y uno `may_alias`, y campos `unused` y `annotate`;
- `__attribute__((aligned(16)))` con un primer campo de bits: `{ int a : 3; int b : 5; char c; }`.

Todo O0–O3, en biblioteca estática y DLL, con y sin encabezado.

Rechazo genuino, compartido con Clang: `_Alignas(2)` en un campo `int`. Clang no lo compila («requested alignment is
less than minimum alignment») y NSC rechaza el productor con el mismo diagnóstico.

## Negativos obsoletos convertidos

El rechazo de `__unaligned` sin encabezado quedó obsoleto. Cada caso pasa a comparación positiva, y el rechazo se
conserva con `_Alignas(2)` en el mismo campo, que Clang tampoco compila:

- `pruebas-tanda-09-precompilado-alineacion`: también el negativo de alineación con bit-fields;
- `pruebas-tanda-09-precompilado-campos-calificados`, `-empaquetado` y `-variadica-anidado`, convertidos con
  `build-claude-t08/tools/transformar-unaligned-replay.py`;
- los 14 `pruebas-tanda-09-sysv-indirecta-*/validar-precompilado.py`, convertidos con
  `build-claude-t08/tools/transformar-unaligned.py`. Cada `variante-unaligned…` pasa a admitida y se agrega una
  `variante-alignas-menor…`.

## Evidencia

| Fixture | Estática | DLL |
|---|---|---|
| `pruebas-tanda-09-precompilado-alineacion` | `A:/temp/nsc-t09-alineacion-0dy5wrso` | `A:/temp/nsc-t09-alineacion-9kph2qol` |

Validación puntual, por decisión del usuario:

- Los 358 guiones de replay precompilado de la tanda 9 en estática (los 4 `precompilado-*` y todos los
  `validar-precompilado`), con el primer cambio (`logs/regresion-d60.log`):
  - pasaron 340;
  - fallaron los 17 negativos `__unaligned` que el cambio dejó obsoletos, más `cpp-variadica-preserve-none`, que es
    línea base (T19-032).
- Los 4 `precompilado-*` en DLL con ese mismo cambio (`logs/regresion-dll-d60.log`): fallaron por los mismos negativos y
  por un `api.dll` falso que faltaba en el negativo nuevo.
- Con los negativos convertidos y la ampliación final (`packed` en campo, pack con bit-fields, más atributos):
  - en estática pasan los 18 (`logs/regresion-d62.log`): los 17 convertidos y el fixture ampliado;
  - en DLL pasan los 4 `precompilado-*` (`logs/regresion-dll-d62.log`).

La regresión global queda para el final.

## Límites

- Un atributo fuera de los admitidos que aparezca en el futuro deja el record fuera del replay sin encabezado, con el
  diagnóstico de siempre; con encabezado pasa.
- La forma reconstruida puede diferir de la declarada (por ejemplo, `#pragma pack(2)` donde se escribió
  `#pragma pack(4)` sin efecto). El layout resultante, que es lo que decide el plan de Clang, es el mismo y se
  compara con el catálogo.
- Los records C++ no cambian: su productor sigue exigiendo records sin atributos.
