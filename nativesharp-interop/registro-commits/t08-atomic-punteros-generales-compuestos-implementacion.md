# T08-004: avance de punteros atómicos C por elemento

El backend usa para punteros atómicos el tamaño de elemento ya calculado por
MIR mediante elementLayout, en lugar de una tabla de tipos escalares. Mantiene
la comprobación de alineación, la exigencia de tamaño no cero y la operación
atómica secuencialmente consistente. Pendientes construcción y validación.
