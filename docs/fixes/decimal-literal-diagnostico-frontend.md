# Diagnóstico de literales decimal en el frontend (NSC-NEW-009)

Un literal decimal distinto de cero con más de 28 decimales, o con un coeficiente que no cabe en 96
bits, pasaba el análisis y fallaba al bajar a LLVM con "LLVM code unit #0 failed: lowering failed:
Stage 34 local initialization failed: decimal literal coefficient or scale is out of range in Stage
34", sin posición de fuente.

El análisis del literal (`PrimaryExpressions.cpp`) aplica ahora `language::parseDecimalLiteral`, la
misma función que usa la bajada, e informa `InvalidNumericLiteral` en el literal:
"invalid decimal literal '<texto>': decimal scale exceeds 28" o "...: decimal coefficient exceeds
96 bits". El literal queda inválido y no llega a la bajada.

`tests/integration/decimal_zero_literal_scale.py` comprueba los dos casos con línea y columna
(`:1:32:`) y el texto exacto; antes esperaba el mensaje de la bajada. Resultado: 22/22 pruebas
`decimal`.
