# Enum C++ de base unsigned int

Clang ejecutaba una muestra con enum global de base `unsigned int`, pero NSC
rechazaba el enumerador y el campo. Reproducciones:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-9sdjwohv` y
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-q_81syiq/estatica-O0-ns-ir.log`.

El inventario admite ahora el tipo y enumeradores con base `unsigned int` de
32 bits, preservando su identidad enum y el tratamiento sin signo del valor.
No se alteran las conversiones específicas de C. El control de rechazo que
antes usaba esa base ahora usa `long long`, aún no implementada.

`pruebas-tanda-10-enum-cpp-unsigned/validar.py` contrastó con Clang 23.1.0 y
ejecutó en O0–O3 los valores 0 y `0xFFFFFFFF`, su copia y lectura/escritura en
un campo de record; rechaza asignar un enumerador de otro tipo. Registros:
`A:/temp/nsc-t10-enum-cpp-unsigned-vfspi7x2`. También pasaron la regresión C
y la muestra global previa (`A:/temp/nsc-t10-enum-cpp-global-wk9qgt1u`) y los
enums con namespace de base `int` (`A:/temp/nsc-t10-enum-cpp-namespace-zuwo4_i8`).

La firma SysV con este campo y su precompilación todavía no están acreditadas.
No se acreditan otras bases ni enums scoped; T09-003 y T10-002 siguen abiertos.
