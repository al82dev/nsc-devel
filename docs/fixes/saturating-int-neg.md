# Negación saturada de int

En `10225909c`, `saturating(-minimum)` envolvía y producía minimum en O0/O2.
La reproducción está en `A:/nsc-fixes-evidencia/int-saturating-neg-before-20261112.log`.

El lowering de negación int saturating utiliza la resta saturada de cero y el
operando. La negación ordinaria y los demás tipos conservan sus rutas. El
ejemplo original no cambió.

El build incremental terminó 0; compilador SHA-256
`9dafe59b7ca23344db5ae5fa4cbbc107068fdf4d6e6ac9c482ca02857a04df87`.
La regresión ejecuta el mínimo, los extremos representables, cero y wrapping
ordinario en O0/O2. Las cinco pruebas de saturación int pasaron 5/5; la suma
solo cubre O0. Las seis pruebas dirigidas `checked` i128 pasaron 6/6. Logs:
`A:/nsc-fixes-evidencia/int-saturating-neg-ctest-20261112.log` y
`int-saturating-neg-i128-controls-20261112.log`. No se acreditan otros tipos
ni plataformas. `CMakeLists.txt` se comparte con interoperabilidad y requiere
revisión al integrar. No se ejecutó batería completa.
