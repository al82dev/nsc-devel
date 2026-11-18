# Conversión checked de i128 a enteros pequeños

En `6e0103be8`, el compilador aislado aceptaba valores fuera del rango al
convertir `i128` en modo checked a `short`, `sbyte`, `byte`, `ushort` y
`char`: el sondeo `checked-i128-small-probe-20261113.log` terminó normalmente
en O0 y O2 para ambos extremos. El backend truncaba esas conversiones sin
aplicar la guarda ya existente para `int` y `long`.

`convertScalar` comprueba ahora esos cinco destinos mediante truncamiento y
extensión según el signo del destino; si el valor restaurado no coincide,
produce el trap de overflow existente. Las conversiones ordinarias y otros
orígenes mantienen su tratamiento anterior. Según la tabla de enteros del
compilador, `char` es un entero sin signo de ocho bits.

Tras compilar incrementalmente, `tests/integration/checked_i128_small.py`
pasó 60 ejecuciones: límites, cero, desbordamientos inferior y superior y
conversión ordinaria de cada destino en O0 y O2. Pasaron los controles
dirigidos de conversión checked a `int` y `long`. Los comandos y resultados
están en `A:\nsc-fixes-evidencia\checked-i128-small-final-20261113.log`,
`checked-i128-small-control-int-20261113.log` y
`checked-i128-small-control-long-20261113.log`. La primera prueba usó por
error un límite de 16 bits para `char`; se corrigió el fixture de acuerdo con
la tabla y se volvió a ejecutar. No se acreditan otros pares de tipos ni otras
plataformas. `OopLlvmFieldsAndConversions.cpp` puede coincidir con cambios de
interoperabilidad y deberá revisarse durante la integración.
