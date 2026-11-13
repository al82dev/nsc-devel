# Enum C++ char observado como firmado

Clang para el destino Win64 de la muestra describe `char` como entero firmado
de 8 bits; NSC rechazaba el enum con esa base. La importación, plan y catálogo
admiten ahora esa representación concreta, conservando identidad y signo. No
se asume que `char` sea firmado en otras configuraciones.

`pruebas-tanda-10-enum-cpp-plain-char/validar.py` contrastó los extremos y la
copia en O0–O3: `A:/temp/nsc-t10-enum-cpp-plain-char-6yw0oom2`.
`pruebas-tanda-09-sysv-cpp-copia-enum-plain-char/validar.py` pasó ocho
comparaciones Clang 23.1.0/NativeSharp en estática y DLL:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-ey16t5yc`. Su prueba precompilada
pasó 16 consumidores con y sin encabezado:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-938j8uyq`.

Pasaron los controles de identidad, ABI y transporte, el plan de un byte y la
regresión C (`A:/temp/nsc-t10-enum-cpp-global-9pvht8q0`). Los controles
negativos usan ahora `wchar_t`, todavía no acreditado.

No se acredita `char` observado como unsigned ni otras formas de ABI. T09-003
y T10-002 permanecen abiertos.
