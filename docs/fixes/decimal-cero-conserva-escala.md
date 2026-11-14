# Un literal decimal cero conserva su escala (NSC-NEW-003)

`DecimalLiteralValue` promete dígitos y escala "verbatim from the text", pero `parseDecimalLiteral`
devolvía escala 0 para todo cero: `0.0m` se almacenaba como `0m`. Una sonda de bits lo mostró
antes del cambio: `0.0m` y `0.00m` daban la palabra de escala 0, mientras `0.10m` conservaba la
escala 2 y un cero construido desde bits con escala 1 dividido entre `-1m` daba escala 2 sin signo.

Consecuencias medidas:

- `nsc.compiler.decimal-multiply-divide` fallaba en `DivZeroResult` O0/O2: `0.0m / -1m` partía de
  `0m`. Fallaba también con HEAD `868c29e87` recompilado.
- `decimal-exact-scale-reduction` y `decimal-reduced-zero-checked` prueban `max + 0.0m`, cuyo
  coeficiente alineado a escala uno supera 96 bits. Con el cero normalizado pasaban sin ejercer la
  reducción que dicen medir.

## Cambio

Un cero conserva `fractionalDigits - exponent`. Un cero no tiene dígitos que perder, así que una
escala mayor que 28 se acota a 28 en vez de rechazarse (antes se aceptaba con escala 0), y una
escala negativa queda en 0. El cero sigue positivo, como antes. Los literales distintos de cero no
cambian.

## Prueba

`tests/integration/decimal_zero_literal_scale.py` (`nsc.compiler.decimal-zero-literal-scale`) lee
la palabra de escala y signo en O0 y O2: `0m`, `0.0m`, `0.00m`, un cero de 28 y uno de 30
decimales, exponentes `0.0e1m` y `0e-3m`, `-0.0m`, `0.0m / -1m`, `0.0m + 0.00m` e igualdad numérica.
Con el analizador anterior falla en la segunda comprobación. El caso inválido conserva el límite de
escala para literales distintos de cero.

Resultado: las 19 pruebas `decimal` pasan, incluidas `decimal-multiply-divide`,
`decimal-exact-scale-reduction` y `decimal-reduced-zero-checked`, que ahora parten del valor
escrito.
