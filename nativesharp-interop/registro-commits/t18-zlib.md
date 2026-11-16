# T18-008 y T18-009: zlib 1.3.2 desde NativeSharp

## Comprobación

Este cambio comprueba una biblioteca C real; no cambia código de producción.

## Selección (T18-008)

zlib 1.3.2 (`A:/al82dev-clang-deps/zlib-1.3.2`) ejercita los contratos C de las tandas 8 y 9 en una API
ampliamente usada: funciones con punteros a búfer y a longitud (`uLongf*`), una cadena constante devuelta, un
struct con punteros, contadores `uLong` y punteros a función (`z_stream`, con `zalloc`/`zfree`) operado por
estado entre llamadas, macros de versión y el asignador por defecto, que usa `calloc`/`free` del runtime de C.

## Evidencia (T18-009)

Fixture nuevo `pruebas-tanda-18-zlib/validar.py`, con y sin `--dll`. Once unidades de zlib compiladas con
Clang 23.1.0 (`-std=c17 -O2`) en una biblioteca estática o en `zlib.dll` (`ZLIB_DLL`); el programa NativeSharp
y un control C hacen las mismas llamadas: `compress` y `uncompress` de 256 bytes, `zlibVersion`, la API de
flujo con dos `z_stream` locales (`deflateInit_` nivel 9, `deflate`, `deflateEnd`, `inflateInit_`,
`inflate`, `inflateEnd`), `crc32` y `adler32`. Resultado 137 en los dos, O0–O3. El local `z_stream` sin
inicializador empieza en cero (`total_in` y `opaque`), como el `memset` del control.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática (`--crt-startup`) | pasa (O0–O3) | `A:/temp/nsc-t18-zlib-h25_0g16` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t18-zlib-0u2oduqr` |

## Límites

- `zalloc = Z_NULL`: asignar `null` a un campo puntero a función se rechaza; el campo queda en cero porque el
  local empieza en cero.
- `sizeof(z_stream)` es `nuint` y `deflateInit_` pide `int`: se convierte explícitamente.
- Las macros de función (`deflateInit`) no se expanden: se llama a `deflateInit_` (T14).
- `gz*` (archivos) no se probó.
