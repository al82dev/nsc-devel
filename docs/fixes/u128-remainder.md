# Resto u128

El resto u128, variante del fallo de enlace i128 descrito en el informe, generaba una referencia a `__umodti3` sin resolver en O0/O2. El sondeo basal está en `A:/nsc-fixes-agent1-temp/u128-divrem-observe-20261112.log`. El diagnóstico no implica por sí solo un defecto de NSLink.

Después de la guarda de cero, la rama específica de resto u128 calcula el cociente con el algoritmo restaurador sin signo y obtiene `a − q*b`. Las rutas de división y de enteros con signo no cambian. La validación dirigida pasó resto u128, divisor cero (129), división u128 y regresiones de división y resto i128 firmados en O0/O2; el fixture cubre máximo, bit alto y divisores 1, 2, máximo y bit alto. No demuestra todas las entradas.

El log `A:/nsc-fixes-agent1-temp/u128-remainder-integrated-20261112.log` conserva comandos y hashes: compilador SHA-256 `e694727b24aa16300046324bbe3036d5a752b0cf01370129e863210559ca3405`, runtime e import libraries estables. El enlace tuvo éxito; no se inspeccionaron objetos para descartar definición estática del helper. El compilador contenía WIP y no certifica árbol limpio. El runner CMake exacto integrado pasó Probe/Zero en O0/O2 con salida 0; artefactos en `A:/nsc-fixes-agent1-temp/u128-remainder-cmake-20261112` y log vacío por diseño al pasar. No se ejecutó CTest.
