# Ítem 355 — Formatos GPU empaquetados / Packed GPU formats

## Resultado / Result

`Rgba8Unorm` y `Rgb10A2Unorm` son carriers de 32 bits con orden de canales, máscaras y accesores
explícitos. El layout puede copiarse directamente a buffers gráficos sin campos ocultos. /
`Rgba8Unorm` and `Rgb10A2Unorm` are 32-bit carriers with explicit channel order, masks, and
accessors. Their layout can be copied directly into graphics buffers with no hidden fields.

## Pruebas / Tests

La prueba fija ambos tamaños en cuatro bytes y valida pack/unpack en O0 y O2 con resultado 42. /
The test pins both sizes to four bytes and validates pack/unpack at O0 and O2 with result 42.
