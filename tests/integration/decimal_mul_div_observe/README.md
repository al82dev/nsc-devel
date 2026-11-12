# Observaciones de multiplicación y división decimal

Estos casos son reproducciones preparadas, no resultados de ejecución. Cada proyecto
es un proceso independiente y llama una función `noinline` para no convertir la prueba
en evaluación de una expresión constante. El runner ejecuta O0 y O2 y registra los
códigos de compilación y ejecución junto con stdout y stderr.

Los documentos históricos `NS304-T15-F04-arithmetic.md`,
`NS304-T15-F05-checked-saturating.md` y `NS304-T15-F07-consteval-lowering.md`
describen, respectivamente, half-up y trap de escala superior a 28, trap o saturación
del producto según política, y representación de 96 bits con signo en el helper.
`NativeSharp3T14NumericTests.cpp` también describe el coeficiente de 96 bits y el signo.
Estos documentos no prueban que el comportamiento observado en la revisión actual
sea correcto ni que el helper sea un oráculo general.

| Caso | Resultado comprobado por el fixture | Base de la expectativa |
|---|---|---|
| MulSign | Dos signos en el producto; salida 0 si coinciden | Signo del producto y signo XOR del helper |
| DivLeftSign, DivRightSign | Cociente negativo; salida 0 | Signo XOR del helper |
| MulWide | Coeficiente 2^64 multiplicado por 1; bits iguales | Capacidad de 96 bits |
| DivWideDividend | Coeficiente 2^64 dividido por 1; bits iguales | Capacidad de 96 bits |
| DivWideDivisor | 2^64 dividido por 2^64 es 1 | Capacidad de 96 bits |
| DivHalfUp | 0.5 / 3 es 0.17 | Redondeo histórico half-up |
| MulScaleOverflow | Escalas 28 + 1; salida 145 | Trap histórico de escala |
| MulCoefficientOverflow | Máximo por 2; salida 145 | Trap histórico de coeficiente |
| MulSaturatingPositive, MulSaturatingNegative | Máximo por ±2 satura a ±máximo | Saturación y signo históricos |
| DivZero, DivZeroScaled | Divisor decimal cero con diferentes representaciones; trap 129 | Nueva decisión para división decimal |
| DivScaleReduction, DivMultiScaleReduction | `M/1.0` y `M/1.000` reducen ceros exactos | Nueva decisión para valor representable |
| DivOverflowChecked, DivOverflowNegativeChecked | Cociente fuera de rango; trap 145 | Nueva decisión para default/checked |
| DivOverflowSaturating | Cocientes ±10M saturan a ±M | Nueva decisión para saturating |
| DivInteriorInexactChecked, DivInteriorInexactSat, DivInteriorNegativeSat | Valor interior no representable; trap 145 | Nueva decisión sin redondeo adicional |
| DivMaxOverTwo, DivRoundingCases, DivNegativeHalfUp | Redondeo half-up y signos | Extensión de regla histórica a signo negativo |
| DivWideNonzeroDivisor | Divisor con bit 64 y parte baja no nula | Capacidad de 96 bits |
| DivZeroResult | Cero redondeado sin bit de signo y con escala histórica | Nueva decisión de representación |
| DivScaleOverflow | Escala objetivo 29; trap 145 | Trap histórico de escala |

Las entradas extremas se construyen desde los bits para aislar la operación del soporte
del parser de literales. Esto requiere `unsafe` únicamente dentro de esos fixtures.
La comparación por bits se utiliza cuando la escala esperada es cero. La salida 1 o 2
identifica una discrepancia cuando se esperaba 0 o un trap; no es un diagnóstico de
causa. Un fallo de compilación, timeout o excepción del proceso se conserva tal como
se recibe. Un caso bloqueado no demuestra el comportamiento aritmético.

`VerifyDecimalMulDivObserve.cmake` exige que el llamador suministre compilador,
linker, runtime y rutas de fuente y salida. Antes de ejecutarlo debe comprobarse la
procedencia y autorización de las herramientas; el runner no las certifica. Sus
temporales se redirigen al directorio de salida. Las reglas nuevas están documentadas como
decisiones delegadas, no se atribuyen a un contrato histórico.
