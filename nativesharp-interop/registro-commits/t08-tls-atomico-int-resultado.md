# Resultado inicial de TLS C atómico int

La construcción incremental de NSC terminó correctamente. El fixture atómico
terminó con código 0: controles C y NativeSharp, IR específico `seq_cst`,
lectura/escritura/suma/resta y pre/post incremento, contención de dos hilos,
casos `const` y `volatile`, rechazo de dirección ordinaria y rechazo explícito
de multiplicación compuesta aún no implementada. Registro:
`A:/nsc-interop-build/tls-abi/atomic-calificadores-validacion.log`.

La suite de sesión Clang y la prueba de identidad de código MIR terminaron con
código 0. No demuestra los demás tipos u operadores atómicos, `ref/out/in`,
metadata para tipos atómicos, DLL o la batería integral. Los cuatro controles
TLS de DLL y los 25 fallos observados en la suite HIR/MIR completa siguen sin
resolver; la tanda permanece abierta.
