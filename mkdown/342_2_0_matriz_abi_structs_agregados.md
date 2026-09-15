# Ítem 342 — Matriz ABI de structs y agregados / Struct and aggregate ABI matrix

## Resultado / Result

La frontera C Win64 queda certificada para agregados de 1, 2, 4, 8, 12, 16, 24 y 64 bytes.
Los cuatro tamaños directos viajan en un registro entero; cualquier otro tamaño usa copia
indirecta para parámetros y `sret` para retornos. / The Win64 C boundary is certified for 1, 2, 4,
8, 12, 16, 24, and 64-byte aggregates. The four direct sizes travel in an integer register; every
other size uses an indirect parameter copy and `sret` returns.

## Cobertura / Coverage

Cada fila cruza simultáneamente parámetro y retorno mediante un `extern C delegate`; así se prueba
la firma publicada y la reconstruida en el call site. Los tamaños 12 y 24 fijan los casos impares,
y 64 representa agregados grandes. / Every row crosses both parameter and return through an
`extern C delegate`, testing the published signature and the reconstructed call-site signature.
Sizes 12 and 24 pin odd aggregate cases, while 64 represents large aggregates.

## Pruebas / Tests

La prueba ejecuta las ocho filas como un único checksum 42 en O0 y O2. Cualquier desacuerdo entre
caller y callee se manifiesta como valor incorrecto o fallo del proceso. / The test executes all
eight rows as one checksum of 42 at O0 and O2. Any caller/callee disagreement appears as an
incorrect value or process failure.
