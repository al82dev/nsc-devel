# T08-002: agregados, promociones con signo y punteros en funciones C variádicas

Fecha: 2026-11-08. Casilla: «Comprobar agregados y otras promociones, incluidas variantes con signo y
punteros, sin extrapolar la muestra anterior» (funciones variádicas, dos DLL TLS homónimas).

## Reproducción

Sonda `claude/sondas/variadic1` (`sonda2.py`, `sonda3.py`), con una DLL C con declaraciones `dllimport`,
en O0:

| Argumento en `...` | Antes |
|---|---|
| `(sbyte)-5`, `(short)-300`, `(byte)250`, `(ushort)65000`, `true` | correcto |
| `(long)-5000000000`, `(uint)4000000000` | correcto |
| enum C con valor negativo | correcto |
| `int*`, `int*` nulo con tipo, puntero a función en una variable | correcto |
| record C de 8 bytes, de 24 bytes y de 3 bytes por valor | `no viable overload of namespace function … accepts the argument types` |
| `null` sin tipo | rechazado |
| grupo de métodos (`twice`) sin tipo destino | rechazado |

Causa del caso de los records: `classifyCVariadicPromotion` solo admitía escalares, punteros, enums y
punteros a función. El lowering de llamadas ya pasaba por `wrapIndirectArgument` cada operando,
incluidos los variádicos. Así, un record C ya tenía el portador Win64 correcto: un registro si mide
1, 2, 4 u 8 bytes y, en otro caso, un puntero a una copia propiedad de la llamada.

## Cambio

- Nuevo `ProjectMethodBodyModel::isCompleteCRecordTypeKey`: verdadero para un struct o union C completo
  e importable.
- `classifyCVariadicPromotion` acepta esos records sin conversión. Esto vale para la llamada directa y
  para la invocación mediante un typedef de función variádico.
- El diagnóstico de los argumentos variádicos en punteros a función ahora menciona «C records».

Un `struct` NativeSharp, un `null` sin tipo y un grupo de métodos sin tipo destino siguen rechazándose.
En C, `NULL` y una función tienen tipo; en NativeSharp, esos dos argumentos no tienen un tipo que
`va_arg` pueda leer, y hay que darles uno (`int* z = null;`, `var f = twice;`).

## Prueba

Nueva: `pruebas-tanda-08-identidad-c/tls-dll-distintas/validar-variadicas-agregados.py`. Resultado:
aprobada en O0–O3.

- Dos DLL `api.dll` homónimas con TLS exportan `var_signed`, `var_wide`, `var_pointer` y `var_record`.
  Cada una devuelve una constante de su propia DLL (3 o 7) si todos los argumentos leídos con `va_arg`
  son correctos. Así, una llamada que llega a la otra DLL o lee una posición equivocada falla.
- Valores: enteros estrechos con y sin signo y `bool`; `long long`, `unsigned` y `double` negativo;
  `int*`, `int*` nulo y puntero a función; records de 8, 24 y 3 bytes seguidos de un `int`, en llamada
  directa y mediante `VarFn` (`int (*)(int, ...)`).
- Un control C con Clang por DLL hace las mismas llamadas. La salida del control es una máscara de bits
  de las llamadas que fallan.
- Las llamadas NativeSharp están en plantillas precompiladas, un productor por DLL, y se consumen sin
  encabezados. `Access` sigue escribiendo y leyendo la TLS de cada DLL. El consumidor se restaura desde
  la caché (`not-found` y luego `hit`) y publica las dos DLL TLS.
- Negativos (O0): un `struct` NativeSharp, `null` sin tipo y un grupo de métodos se rechazan.

Regresión:
- `validar-variadicas.py` O3 (muestra anterior): aprobada.
- `regresion-callbacks.cmd`: aprobada salvo T08-017-02a, que ya fallaba antes.

## Observación

La primera versión de la prueba leía la TLS desde funciones de una DLL sin importaciones. Por el
problema ya abierto en T08-002 (el valor inicial de TLS en una DLL sin importaciones), devolvía
valores incorrectos. Por eso las funciones variádicas devuelven ahora una constante por DLL, y la TLS
se comprueba con `Access`, como en las muestras anteriores.
