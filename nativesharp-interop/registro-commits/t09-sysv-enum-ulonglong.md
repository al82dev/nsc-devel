# Enum C++ unsigned long long en llamada SysV

Clang ejecutaba un record con enum global de base `unsigned long long`, pero
NSC rechazaba el enumerador y su campo. La importación y el catálogo conservan
ahora base, identidad y valor de 64 bits sin signo; el plan SysV coteja el
tipo y layout con el `i64` físico.

`pruebas-tanda-10-enum-cpp-ulonglong/validar.py` contrastó cero y
`0xFFFFFFFFFFFFFFFF`, copia y campo en O0–O3:
`A:/temp/nsc-t10-enum-cpp-ulonglong-bmq6xbx8`. La muestra scoped también
verificó ese máximo como miembro de `enum class`:
`A:/temp/nsc-t10-enum-cpp-scoped-mk9n0w7s`.

`pruebas-tanda-09-sysv-cpp-copia-enum-ulonglong/validar.py` pasó ocho
comparaciones Clang 23.1.0/NativeSharp en estática y DLL con el máximo:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-xa8pisdc`. La prueba precompilada pasó
16 consumidores con y sin encabezado:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-pqoyz8ii`.

Pasaron los controles de identidad, ABI y transporte, el plan de 64 bits y
la regresión C (`A:/temp/nsc-t10-enum-cpp-global-odhum8t5`). Los controles
negativos previos usan ahora `signed char`, aún no acreditado.

La evidencia SysV corresponde a un enum global no scoped y una firma
concreta; otras formas siguen pendientes. T09-003 y T10-002 permanecen abiertos.
