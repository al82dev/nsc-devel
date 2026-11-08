# Tanda 8: incremento de enum C no atómico

La habilitación semántica de `++` y `--` para enums C no se limita a los
atómicos. Se añade una global C ordinaria con valor negativo y se contrastan
el resultado de incremento sufijo, el valor almacenado y decremento prefijo
con una referencia C. Pasó O0–O3 junto con ambas variantes del fixture.

Evidencia: `A:\nsc-interop-build\tanda08-atomic-enum-global-951-31393` y
`A:\nsc-interop-build\tanda08-atomic-enum-tls-951-31393`. La global ordinaria
no es TLS; el segundo modo aplica a las demás variables atómicas del fixture.
No se acreditan otras formas de almacenamiento ni todos los anchos.
