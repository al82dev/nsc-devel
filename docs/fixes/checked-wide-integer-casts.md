# Conversiones checked desde enteros de 128 bits

En `13e204dcd`, `checked-wide-probe-20261113.log` registra conversiones
fuera de rango de `i128` y `u128` que terminaban normalmente en O0/O2,
incluidas conversiones entre ambos tipos del mismo ancho. La guarda previa
solo cubría ciertos destinos de `i128` y el retorno para anchos iguales
evitaba comprobar el cambio de signo.

En `convertScalar`, las conversiones checked desde los enteros incorporados
de 128 bits a otros enteros incorporados comprueban ahora la conservación de
los bits y la validez del signo. La comprobación de signo adicional evita que
un valor `u128` con el bit alto activo coincida accidentalmente con una
extensión con signo. Las conversiones ordinarias, enumeraciones y orígenes
distintos conservan su ruta anterior. El overflow usa el trap existente.

El compilador incremental aislado pasó 290 ejecuciones de
`tests/integration/checked_wide_integer_casts.py`: cero y uno, límites
representables, desbordamientos y conversión ordinaria, con destinos enteros
incorporados en O0/O2. Los controles previos para `i128` a `int`, `long` y
enteros pequeños también pasaron. Los comandos y salidas están en
`A:\nsc-fixes-evidencia\checked-wide-final-20261113.log` y los archivos
`checked-wide-control-*-20261113.log` de esa carpeta. Las comprobaciones son
para Windows x64; no acreditan otras plataformas ni conversiones a enum.
`OopLlvmFieldsAndConversions.cpp` es un archivo compartido que debe revisarse
al integrar interoperabilidad.
