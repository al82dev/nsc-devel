# Resta saturada de int

En `c0aa5e2d5`, `saturating(maximum - (-1))` y `saturating(minimum - 1)`
producían wrapping en O0/O2. Los controles independientes están en
`A:/nsc-fixes-evidencia/int-saturating-sub-before-20261112.log` y
`A:/nsc-fixes-evidencia/int-saturating-sub-negative-before-20261112.log`.

`applyIntegerBinaryOperation` utiliza el intrinsic de resta saturada firmada
para `int` bajo la política saturating. La suma conserva su ruta y la resta
ordinaria conserva wrapping; otros tipos y operaciones no cambian. La fuente
original del ejemplo 48 permanece intacta.

El build incremental terminó 0; compilador SHA-256
`2091ef8e3b21e7eccdc7452eb69624ed1c01487d44f68ccd8eb8b0b56efd5f0c`.
La nueva prueba ejecuta límites, valores representables, signos y wrapping
ordinario en O0/O2; pasó junto con la regresión existente de suma (2/2) en
`A:/nsc-fixes-evidencia/int-saturating-sub-ctest-20261112.log`. La prueba de
suma anterior solo cubre O0. No acredita otros tipos, operaciones o plataformas.
`CMakeLists.txt` es compartido con interoperabilidad y deberá revisarse al
integrar. No se ejecutó batería completa.
