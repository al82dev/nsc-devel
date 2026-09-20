# NativeSharp 2.0 self-hosting — tanda 33: texto nativo

## Identidad

| Campo | Valor |
|---|---|
| Estado | `DIRECTED-PASSED` — baseline acumulado pendiente |
| Gate | `SH31` |
| Fecha | 2026-09-20 |
| Rama | `development-claude` |
| Commit de entrada | `338393403` |
| Commit de cierre | este commit (`selfhost(tanda-33): emit native text literals`) |

## Implementación

La binding LLVM-C expone `LLVMBuildGlobalStringPtr`. El backend usa `EmitGlobalString` para emitir
literales terminados en cero con nombre interno derivado de `StableHash`; los bytes UTF-8 vienen de
`CString`, la misma frontera nativa usada por el resto del compilador.

Se añadieron tres caminos de lowering:

- `CharacterLiteralExpression`, incluida decodificación de escapes básicos;
- `StringLiteralExpression` a global `char*`;
- concatenación constante de dos literales, plegada por semántica y materializada como un único
  global (`"Native" + "Sharp"` -> `"NativeSharp"`).
- comparación `==`/`!=` entre literales, plegada a `bool` antes de codegen.

`ConvertScalar` reconoce la conversión semántica existente
`implicit-string-literal-to-char-pointer`; la indexación reutiliza el GEP tipado de punteros y
produce un lvalue `char`.

## Gate SH31

`native-text/Program.ns` verifica literales `char`, indexación, concatenación y un payload UTF-8
`"México"`. El objeto contiene `4D C3 A9 78 69 63 6F 00`; la ejecución comprueba los índices de byte
0 y 3, además de `NativeSharp`, y devuelve 42.

La matriz O0–O3 × LLD/NSLink pasa completa: **12.88 s** en la corrida dirigida inicial. El gate
también compara la semántica byte a byte y exige el payload UTF-8 en `.rdata`.

## Archivos

- `lib/NativeSharp.LLVM.C/LLVMApi.ns`: `LLVMBuildGlobalStringPtr`;
- `SemanticAnalyzer`: plegado de concatenación literal;
- `LlvmBackend`: char, escapes, strings globales y conversión a `char*`;
- fixture, contrato, verificador y gate SH31.

La clase de biblioteca `string`/`CString` conserva su modelo declarado; esta tanda cierra la
representación nativa que necesita el bootstrap (bytes UTF-8, literal global, conversión e
indexación) sin introducir heap oculto.

- [x] `char` y escapes básicos.
- [x] literales globales UTF-8 terminados en cero.
- [x] conversión a `char*`, concatenación/comparación literal e indexación.
- [x] O0–O3 con ambos linkers.
- [ ] baseline acumulado final.
