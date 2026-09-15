# Ítem 353 — Matrices nativas / Native matrices

## Resultado / Result

`Float32x3x3` y `Float32x4x4` son valores column-major de 36 y 64 bytes construidos sobre los
vectores compactos y SIMD del núcleo. Ambos exponen transformación matriz-vector y multiplicación
matriz-matriz sin almacenamiento oculto. / `Float32x3x3` and `Float32x4x4` are 36- and 64-byte
column-major values built on the core compact and SIMD vectors. Both expose matrix-vector transform
and matrix-matrix multiplication with no hidden storage.

## Pruebas / Tests

La prueba fija los tamaños ABI con `static_assert` y ejecuta una transformación 4x4 en O0 y O2 con
resultado 42. / The test pins both ABI sizes with `static_assert` and executes a 4x4 transform at O0
and O2 with result 42.
