# Ítem 356 — Swizzle y shuffle / Swizzle and shuffle

## Resultado / Result

`Float32x4.SwizzleWzyx` invierte carriles de una fuente y `ShuffleLowHigh` combina los dos carriles
bajos de la primera con los altos de la segunda. Ambas firmas bajan a `shufflevector <4 x float>`. /
`Float32x4.SwizzleWzyx` reverses lanes from one source and `ShuffleLowHigh` combines the first
source's low lanes with the second source's high lanes. Both lower to `shufflevector <4 x float>`.

## Pruebas / Tests

La prueba ejecuta ambos patrones en O0/O2 con resultado 42 e inspecciona el IR vectorial. / The test
executes both patterns at O0/O2 with result 42 and inspects the vector IR.
