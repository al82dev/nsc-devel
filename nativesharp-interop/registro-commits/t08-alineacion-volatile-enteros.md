# Alineación de globales volatile enteros

T08-015: el cálculo de evidencia HIR reconoce globales enteros importados de
8, 16, 32 y 64 bits, solo con tamaño y alineación compatibles y la opción MS
volatile activa en la sesión de Clang. El dato ausente sigue sin demostrar
elegibilidad. La emisión de los nuevos tamaños se incorpora por separado y la
validación se realizará de manera agrupada.
