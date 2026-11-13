# Enum C++ long long en llamada SysV

Clang ejecutaba un record con enum global `long long`, pero NSC rechazaba el
enumerador y después la firma del puntero. Se proyectan el tipo y sus valores
de 64 bits con signo y se concilian identidad, base y layout con el `i64`
físico, tanto en la ruta viva como en el catálogo vigente.

`pruebas-tanda-10-enum-cpp-longlong/validar.py` pasó en O0–O3 para valores
positivos y negativos fuera de 32 bits, copia y acceso a campo:
`A:/temp/nsc-t10-enum-cpp-longlong-835acvoe`.
`pruebas-tanda-09-sysv-cpp-copia-enum-longlong/validar.py` pasó ocho
comparaciones Clang 23.1.0/NativeSharp con −1099511627776 en estática y DLL:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-ou6tx90f`. Su prueba precompilada
pasó 16 consumidores con y sin encabezado:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-ggd6c4lv`.

Pasaron los controles de identidad, ABI y transporte, el control del plan de
64 bits y la regresión C (`A:/temp/nsc-t10-enum-cpp-global-c0fj32fn`). El
control negativo previo se trasladó a `unsigned long long`, aún no acreditado.

La evidencia corresponde a un enum global no scoped y una firma concreta;
no prueba los extremos de 64 bits ni otras formas. T09-003 y T10-002 siguen
abiertos.
