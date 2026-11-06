# T08-007 / T08-015: orden de globales volatile flotantes

Clang 23.1.0 para Windows x64 emitió `load atomic volatile float acquire` y
`store atomic volatile float release` en un ejemplo directo. NSC reconocía
el orden MS para enteros y punteros, pero no para `float` ni `double`. Se
extienden las dos comprobaciones existentes al tamaño natural de esos tipos,
manteniendo los requisitos de modo, tamaño y alineación obtenidos de Clang.
Los accesos sin esas garantías conservan la ruta anterior. Se prepararán
casos comparativos y la validación de la tanda; el comportamiento todavía no
está acreditado por una ejecución mixta.
