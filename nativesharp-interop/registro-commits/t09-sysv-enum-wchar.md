# Enum C++ wchar_t en llamada SysV

Clang para el destino Win64 de la muestra describe `wchar_t` como unsigned de
16 bits; NSC rechazaba el enum con esa base. La importación, plan y catálogo
admiten esa representación concreta, conservando identidad y signo. No se
asume el mismo layout en otros destinos.

`pruebas-tanda-10-enum-cpp-wchar/validar.py` contrastó 0 y 65535, copia y
campo en O0–O3: `A:/temp/nsc-t10-enum-cpp-wchar-8ktur27m`.
`pruebas-tanda-09-sysv-cpp-copia-enum-wchar/validar.py` pasó ocho comparaciones
Clang 23.1.0/NativeSharp en estática y DLL:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-b1jyz8v_`. Su prueba precompilada pasó
16 consumidores con y sin encabezado:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-ctv0krw7`.

Pasaron identidad, ABI, transporte, el plan y la regresión C
(`A:/temp/nsc-t10-enum-cpp-global-lsr_oed5`). Los controles negativos usan
`char8_t`, todavía no acreditado.

La evidencia SysV corresponde a un enum global no scoped y una firma concreta;
otras formas y destinos siguen pendientes. T09-003 y T10-002 permanecen abiertos.
