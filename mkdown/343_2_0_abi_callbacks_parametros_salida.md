# Ítem 343 — ABI de callbacks y parámetros de salida / Callback and output-parameter ABI

## Resultado / Result

Los callbacks `extern C` conservan los modos `ref`, `out` e `in` como direcciones nativas en la
frontera Win64. La distinción sigue siendo semántica: `ref` entra inicializado y es mutable, `out`
debe ser escrito, e `in` es visible como sólo lectura. / `extern C` callbacks preserve `ref`, `out`,
and `in` modes as native addresses at the Win64 boundary. Their distinction remains semantic:
`ref` arrives initialized and mutable, `out` must be written, and `in` is read-only.

## Cobertura / Coverage

La firma dirigida combina un escalar `ref`, un struct `out` y un struct `in` en la misma llamada.
Esto fija tanto la posición física de los punteros como las reglas de inicialización, escritura y
no mutación del frontend. / The focused signature combines a scalar `ref`, a struct `out`, and a
struct `in` in one call. This pins both physical pointer placement and the frontend's
initialization, write, and non-mutation rules.

## Pruebas / Tests

La prueba crea el callback, lo invoca por su function pointer en O0 y O2, y verifica el valor
actualizado, el agregado de salida y la preservación de la entrada mediante checksum 42. / The
test creates the callback, invokes it through its function pointer at O0 and O2, and verifies the
updated value, output aggregate, and preserved input through checksum 42.
