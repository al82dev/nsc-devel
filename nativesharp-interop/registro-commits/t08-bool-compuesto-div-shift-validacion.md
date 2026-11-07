# T08-015: validación dirigida de división y desplazamientos de `_Bool` C

La construcción incremental de `nsc` terminó con código 0; registro
`A:/nsc-interop-build/t08-bool-div-shift-build.log`. El grupo
`pruebas-tanda-08-record-c/validar.cmd` terminó con código 0; registro
`A:/nsc-interop-build/t08-bool-div-shift-record.log`.

El fixture ejecutó `/=`, `%=`, `<<=` y `>>=` con operandos positivos válidos
en O0–O3, junto con los demás operadores preparados y el control negativo de
bool nativo. El verificador confirmó dieciséis lecturas acquire y escrituras
release de byte, once promociones y normalizaciones i32, y presencia de
`sdiv`, `srem`, `shl` y `ashr`. La inspección del IR confirmó que esas
operaciones usan las promociones del caso probado.

La evidencia se limita al arreglo C global volatile en modo MS, operandos
`int` y casos definidos probados. T08-015 y la validación integral posterior
siguen pendientes. No se modificó NSLink.
