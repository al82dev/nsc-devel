# Limpieza al destruir un iterador async cancelado

`Cancel()` sobre un iterador detenido en un `yield` conserva la limpieza para
el siguiente avance. Antes de este cambio, `Cancel(); Drop()` sin ese avance
omitía el `finally` y la destrucción del objeto local. En la reproducción con
un `Guard` owned, el control `Drop()` directo devolvía 42 y `Cancel(); Drop()`
devolvía 30 en O0 y O2, con ambos contadores de limpieza en cero. Comando y
salida: `A:/nsc-fixes-evidencia/cancel-drop-before-advance-trace-20261112.cmd`
y `.log`.

`Cancel()` codifica el yield pendiente como `kCancelStateBase - id` con id
positivo. `lowerAsyncGeneratorDrop` solo reconocía estados positivos, por lo
que ignoraba ese estado negativo. Ahora reconoce también los estados inferiores
a `kCancelStateBase` y conserva el estado ya cancelado al invocar el poll de
limpieza. Los estados inicial y terminales conocidos no entran en esa rama.

La regresión `tests/integration/async_cancel_drop_cleanup.py` verifica limpieza
y destrucción una vez: Drop directo, Cancel y Drop, cancelación repetida,
cancelación en el segundo yield, avance posterior a Cancel, Drop implícito,
iterador sin iniciar, terminado normalmente y después de un error capturado.
Antes del build cuatro variantes fallaron en O0/O2 y las restantes entonces
ejecutadas pasaron (`cancel-drop-regression-before-20261112.log`). Tras el
build incremental, las nueve variantes devolvieron 42 en O0/O2, y las ocho
ejecuciones del ejemplo 88 también devolvieron 42. Comandos y logs en
`A:/nsc-fixes-evidencia/cancel-drop-regression-after-20261112.cmd`,
`example88-cwd-after-20261112.cmd` y sus `.log`. El ejecutable usado procede
del build configurado para esta copia, con SHA-256
`7df4b193b39ac3f176b829b87184eaa38360c35345c1fb7ee1e08318557a73fd`;
el log incremental es `cancel-drop-build-20261112.log`. El control de error
comprueba destructores y no mide el valor interno del estado terminal. El test
se registró en CMake después de esas ejecuciones directas; dicho registro no se
ha ejecutado todavía.

Una repetición independiente registró el comando, código de salida 0 y las
18 salidas 42, además del mismo SHA-256 del compilador antes y después:
`A:/nsc-fixes-evidencia/cancel-drop-independent-verification-20261112.log`.
Ese registro no calcula hashes del runtime ni de las bibliotecas de importación.

Este cambio no resuelve la publicación de un yield adicional en finally
anidados ni acredita un Drop que deba completar una limpieza suspendida por
`await`. La regresión independiente de cancelación anidada sigue fallando en
O0/O2 (`nested-cancel-regression-after-20261112.log`). El build es incremental;
los resultados no certifican una reconstrucción limpia ni el SDK binario.
