# Ítem 332 — Promociones C para varargs / C vararg promotions

## Resultado / Result

Las llamadas a imports `extern C` variádicos enlazan el prefijo fijo con las reglas ordinarias y
aceptan después una cola posicional por valor. Cada expresión se evalúa una vez en orden de fuente;
el orden físico conserva primero los parámetros fijos y después la cola.

Variadic `extern C` imports now bind their fixed prefix normally and accept a positional by-value
tail. Every expression is evaluated once in source order, while the physical call order retains
the fixed prefix followed by the tail.

## Promociones / Promotions

- `float` se convierte explícitamente a `double`.
- `bool`, `byte`, `sbyte`, `char`, `short`, `ushort` y enums de menos de 32 bits se convierten a
  `int`.
- enums de 32/64 bits usan su entero compatible; los demás enteros de ese ancho, `double`,
  punteros crudos y delegates `extern C` mantienen su representación.
- cadenas y referencias administradas, arrays, structs, delegates administrados, ownership,
  argumentos de cola nombrados y formas `ref/out/in` se rechazan antes de HIR.

Las conversiones quedan materializadas en HIR y MIR, de modo que LLVM recibe argumentos ya
promovidos bajo el prototipo `retorno (prefijo, ...)`. `LibraryReference` utiliza exactamente la
misma ruta después de reconstruir la marca de NSMETA 1.19.

## Evidencia / Evidence

`Item332CVarargPromotionTests.cpp` cubre HIR, LLVM O0/O2, todos los grupos de promoción, orden con
parámetros fijos nombrados, llamada sin cola, límites negativos y consumo precompilado. Las pruebas
adyacentes de resolución de llamadas, delegates, metadata y enums permanecen verdes. El punto 333
queda responsable de certificar la colocación física específica de registros Win64.
