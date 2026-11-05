# Tanda 08: tipo de expresión de enumeradores

El snapshot conserva por separado el tipo de la expresión de cada enumerador y
el tipo de almacenamiento del enum. Son conceptos distintos, especialmente
entre C y C++; almacenar únicamente el segundo podía producir conversiones o
resoluciones equivocadas. El dato aún no publica la constante en NSC y queda
pendiente de validación agrupada.
