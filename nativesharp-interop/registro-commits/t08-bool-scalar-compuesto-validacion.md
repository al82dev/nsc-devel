# T08: validación de operación compuesta en `_Bool` C escalar

La construcción incremental de `nsc` terminó con código 0; registro
`A:/nsc-interop-build/t08-bool-scalar-compuesto-build.log`. El grupo
record-c terminó con código 0 en O0–O3; registro
`A:/nsc-interop-build/t08-bool-scalar-compuesto-record.log`.

La matriz escalar ejecutó las mismas operaciones y operandos bool, enteros y
flotantes preparados para el arreglo; C comprobó el valor final. El verificador
IR confirmó 68 cargas acquire y escrituras release de byte y las promociones
enteras y flotantes esperadas. La matriz anterior de arreglo y el rechazo de
bool nativo permanecieron en el grupo. El validador TLS atómico terminó con
código 0; registro `t08-bool-scalar-compuesto-atomic.log` en el mismo directorio.

Esto cubre los casos preparados para el global C `volatile _Bool` en modo MS;
no acredita otros orígenes, todos los modos flotantes ni T08 completa. La
validación integral posterior sigue pendiente y NSLink no se modificó.
