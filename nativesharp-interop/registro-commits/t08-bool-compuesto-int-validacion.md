# T08-015: validación dirigida de `_Bool` C compuesto con `int`

La construcción incremental de `nsc` en `A:/nsc-interop-build/tanda1-ON`
terminó con código 0; registro `A:/nsc-interop-build/t08-bool-compuesto-int-build.log`.
`pruebas-tanda-08-record-c/validar.cmd` terminó con código 0 en O0–O3;
registro `A:/nsc-interop-build/t08-bool-compuesto-int-record.log`.

Los controles ejecutaron `+=`, `-=`, `*=`, `&=`, `|=` y `^=` con operandos
`int`, incluidos valores que distinguen la promoción C a i32 de una conversión
previa a bool. El verificador comprobó once lecturas acquire y escrituras
release de byte y siete promociones y normalizaciones a i32. El control
negativo de bool nativo permaneció en el mismo grupo.

La evidencia se limita al arreglo C global volatile de modo MS y a estos
operandos y operaciones. Los demás tipos y operadores, T08-015 y la validación
integral posterior a estos cambios siguen pendientes. NSLink no se modificó.
