# Overflow checked decimal tras reducción exacta

`checked(MaxDecimal + 1.0m)` y `checked(-MaxDecimal - 1.0m)`, así como sus formas predeterminadas, continuaban sin trap en O0/O2. Ambos resultados son enteros exactamente reducibles a escala cero y exceden el coeficiente de 96 bits; no requieren una decisión de redondeo. El baseline está en `A:/nsc-fixes-agent1-temp/decimal-positive-scale-policy-baseline-20261112.log`.

La guarda de `DecimalOverflow` (145) para suma/resta comprueba ahora la escala después de las reducciones exactas. Solo actúa en `checked` y la política predeterminada cuando esa escala llega a cero y la magnitud aún excede 96 bits. Si la reducción se detiene con escala positiva, el comportamiento anterior se conserva; no se decide redondeo ni se modifica `saturating`.

La validación dirigida pasó 22 ejecuciones O0/O2: checked/predeterminado para suma y resta, control representable, overflow a escala cero y regresiones de reducción, coeficientes anchos, signos y orden. Log `A:/nsc-fixes-agent1-temp/decimal-reduced-zero-checked-integrated-20261112.log`, compilador SHA-256 `067eab1245daa72a8af80d2a4405882547fa893f3a0c35493fe5d415e33bebfa` y hashes estables de runtime/import libraries. El build contiene WIP y no certifica árbol limpio.

Las observaciones de `saturating` y del fallback parcial/no divisible se conservaron por separado; no se convierten en regresiones ni contrato. El overflow no reducible a escala cero y el redondeo siguen pendientes. El runner CMake exacto integrado pasó diez casos O0/O2 con salida 0; artefactos en `A:/nsc-fixes-agent1-temp/decimal-reduced-zero-cmake-20261112` y log vacío por diseño. No se ejecutó CTest.
