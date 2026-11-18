# T19-042: tipos como argumento de macros de función C llamadas desde NativeSharp

Fixture nuevo: `pruebas-tanda-19-macros-tipos/validar.py`, con y sin `--dll`.

## Defecto

Una macro de función C que recibe un tipo (`SIZE(int)` con `sizeof(t)`, `CAST(x, t)`, `ZERO(t)`) no se podía
llamar: NativeSharp rechazaba la sintaxis («expected expression»), porque un argumento de llamada solo podía ser una
expresión. Clang la acepta.

## Contrato

1. **Qué entra.** Un tipo predefinido de NativeSharp, con o sin `*`, que ocupa todo un argumento de llamada
   (`int`, `long`, `byte*`, `ulong*`, `int**`, `float`, `double`, `void*`). El parser lo lee como
   `TypeArgumentExpression` (en `Parser::parseArgument` y en la máquina iterativa de postfijos).
2. **Macro de función C.** El argumento llega a la macro como tokens (T19-040), con su escritura C: `bool` es
   `_Bool`, `byte` es `unsigned char`, `sbyte` es `signed char`, `long`/`nint` son `long long`, `ulong`/`nuint` son
   `unsigned long long`. No se analiza ni se pasa como valor; la llamada tiene su propio envoltorio.
3. **Ausencia.** Un tipo NativeSharp sin escritura C (`char`, `string`) no tiene texto y se rechaza con su motivo. Un
   nombre de tipo (`SIZE(Pt)`) sigue leyéndose como identificador: límite.
4. **Error genuino, también en Clang.** Un tipo como argumento de algo que no es una macro C (`hstr(int)`): «a type
   is not a value: only a C function-like macro takes a type argument» (Clang: «expected expression»).
5. **Sin uso.** Una macro sin tipos (`SQ`) y las macros de T19-003, T19-040 y la tanda 14 no cambian.

## Defecto de T19-040 corregido

Con un argumento de tokens seguido de uno de valor (`BYTES(float, 3)`), el análisis de argumentos tomaba los hijos de
la llamada por posición en lugar de la lista sin tokens, así que analizaba `float` como valor y la llamada llegaba a
HIR sin destino. `analyzeNamespaceFunctionArguments` usa ahora la lista sin tokens; para cualquier otra llamada es la
misma lista.

## Validación

- `pruebas-tanda-19-macros-tipos`, estática y DLL: NativeSharp y C devuelven 779460, O0-O3, directo y desde la
  plantilla precompilada con y sin encabezado; 16 llamadas a envoltorios; el rechazo.
- `pruebas-tanda-19-macros-cadena-pegado` (estática y DLL), `pruebas-tanda-19-macros` y `pruebas-tanda-14-macros`
  (estática): pasan.
- Logs: `build-claude-t08/logs/regresion-d68.log` y `regresion-dll-d68.log`.
