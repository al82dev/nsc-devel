# T08-015: controles de `_Bool` C compuesto con flotantes

El fixture prepara `+=`, `-=`, `*=` y `/=` con `float` y `double`; comprueba
valores fraccionarios, cero y conversión de NaN generado por 0/0 flotante. El
verificador de IR exige nueve conversiones flotantes y comparaciones `une`,
además de los accesos de byte. Se conserva el control de bool nativo.

Todavía no se ha construido ni ejecutado esta versión; la semántica del entorno
de coma flotante y las configuraciones de compilación deberán comprobarse en
la validación antes de acreditar el caso. No cierra T08-015 ni la tanda.
