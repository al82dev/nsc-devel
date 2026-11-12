# División i128 con divisor extremo

La comparación basal de `2^63 / (2^64−1)` dio un cociente distinto de cero en O0/O2. La guarda de `correctedSignedDivisor` construye las constantes de ancho 128 mediante valores de 64 bits sin extensión de signo, por lo que ese par coincidía erróneamente. La reproducción, los controles y hashes están en `A:/nsc-fixes-agent1-temp/i128-divrem-baseline-20261112.log`.

La división i128 usa ahora directamente el divisor original después de la guarda de cero y llama al algoritmo restaurador que ya se utilizaba. El algoritmo opera con aritmética i128 sin marcas de overflow y conserva el comportamiento observado de `MinValue / -1`. Los demás tipos y el helper compartido permanecen iguales; el otro backend que lo llama no se ha verificado con i128.

Tras la compilación incremental, el par, `MinValue / -1`, divisor cero, la regresión previa de división y la igualdad decimal pasaron en O0/O2. Cada caso de cero terminó con código 129; log `A:/nsc-fixes-agent1-temp/i128-division-integrated-20261112.log`, compilador SHA-256 `b9378d04238bc607b82b7e742d68e1668d602e701c6dd8bdac88bdf47ba21d91`. El compilador tenía WIP; no es validación sobre un árbol limpio. El resto i128 es un defecto distinto y continúa pendiente.

El runner CMake exacto incorporado se ejecutó desde la copia principal y
terminó 0 para las seis combinaciones Probe/Min/Zero en O0/O2. Los seis
ejecutables están en `A:/nsc-fixes-agent1-temp/i128-divisor-cmake-20261112`;
el log del runner es vacío por diseño cuando pasa. No se ejecutó CTest.
