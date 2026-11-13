# Enum C++ unsigned short en llamada SysV

Clang ejecutaba un record con enum global `unsigned short`, pero NSC rechazaba
el enumerador, el campo y después la firma del puntero. Se proyectan el tipo y
sus valores de 16 bits sin signo y se concilian identidad, base y layout con el
`i16` físico, tanto en la ruta viva como en el catálogo vigente.

`pruebas-tanda-10-enum-cpp-ushort/validar.py` pasó en O0–O3 para 0 y 65535,
copia y acceso a campo: `A:/temp/nsc-t10-enum-cpp-ushort-qr073kqk`.
`pruebas-tanda-09-sysv-cpp-copia-enum-ushort/validar.py` pasó las ocho
comparaciones Clang 23.1.0/NativeSharp con 65535, en estática y DLL:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-h3u40kg8`. Su prueba precompilada
pasó 16 consumidores con y sin encabezado:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-tnn2ce1x`.

El control del catálogo verificó base y valor máximo; pasaron identidad, ABI,
transporte, la regresión precompilada `short`
(`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-rnfqtkav`) y la
regresión C (`A:/temp/nsc-t10-enum-cpp-global-upq5cij6`).

La evidencia corresponde a un enum global no scoped y una firma concreta;
otras bases y formas siguen pendientes. T09-003 y T10-002 permanecen abiertos.
