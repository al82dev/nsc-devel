# Negación checked de i128

En `d7152264d`, `checked(-minimum)` continuaba con wrapping en O0/O2; el
control representable pasó. Comandos y resultados anteriores están en
`A:/nsc-fixes-evidencia/i128-checked-neg-before-20261112.log`.

Para negación `checked` de `i128`, el lowering usa la resta comprobada de
cero menos el operando y produce el trap de overflow entero 147 para el
mínimo. La negación ordinaria y los otros tipos conservan su ruta previa.

El build incremental terminó 0 y produjo el compilador aislado SHA-256
`b12ef2f31e0a9331072f41193a7f4f6c5c3d0e8431eeebebff30f8440b9dbb09`.
El runner cubre en O0/O2 el overflow, cero, ambos signos representables y
wrapping ordinario. Pasaron las seis regresiones dirigidas i128 checked
(`A:/nsc-fixes-evidencia/i128-checked-neg-ctest-20261112.log`). No acredita
otros tipos ni plataformas. `CMakeLists.txt` es compartido con la rama de
interoperabilidad y requiere revisión al integrarse. No se ejecutó batería
completa ni se alteró el ejemplo original del reporte.
