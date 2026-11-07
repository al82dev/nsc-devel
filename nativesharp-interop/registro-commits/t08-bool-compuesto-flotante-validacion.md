# T08-015: validación de `_Bool` C compuesto con flotantes

La construcción incremental de `nsc` terminó con código 0; registro
`A:/nsc-interop-build/t08-bool-flotante-build.log`. El grupo
`pruebas-tanda-08-record-c/validar.cmd` terminó con código 0 en O0–O3;
registro `A:/nsc-interop-build/t08-bool-flotante-record.log`.

El fixture comprobó `+=`, `-=`, `*=` y `/=` con float o double, incluyendo
valores fraccionarios, cero y NaN. El verificador confirmó nueve promociones
flotantes, nueve comparaciones `fcmp une` y 68 cargas acquire y escrituras
release de byte para el conjunto completo. El control negativo de bool nativo
permaneció en el mismo grupo.

Los casos probados usan el arreglo C global volatile en modo MS y la
configuración de compilación de ese fixture; no certifican todos los modos de
coma flotante ni otros orígenes. T08-015 y la validación integral posterior
siguen pendientes. No se modificó NSLink.
