# T08: lectura y escritura de TLS C atómico estrecho

Se proyectan enteros C atómicos TLS con signo y sin signo de 8 y 16 bits cuando
Clang acredita tamaño y alineación adecuados. La lectura y la asignación usan
instrucciones LLVM atómicas `seq_cst` del ancho y alineación correspondientes.
Los operadores compuestos siguen rechazados por el backend: NSC exige convertir
el operando derecho al tipo estrecho y no conserva el caso de promoción de C
demostrado en `t08-tls-atomico-promocion.md`. El rechazo se verifica y no se
presenta como interoperabilidad completa.

La construcción incremental de `nsc` y `atomic/validar.cmd` pasaron. En O0–O3
se escribieron y leyeron `short`, `signed char` y `unsigned short`; el IR se
verificó por símbolo, ancho y alineación. También pasó el control C de promoción
y el rechazo de las operaciones pendientes. Registros en
`A:/nsc-interop-build/tanda08-atomic-narrow-build.log` y
`A:/nsc-interop-build/tanda08-atomic-narrow-final.log`. Faltan los operadores
compuestos, otros tipos y el gate completo de T08.
