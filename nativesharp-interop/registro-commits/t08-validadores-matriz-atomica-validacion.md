# T08: validación de los controles de la matriz atómica

Los nueve validadores modificados pasaron sus rutas normales, incluidas las
matrices O0–O3, controles IR y rechazos que corresponden a cada uno. Dos
copias aisladas provocaron, respectivamente, un error de emisión y una
aceptación inesperada en un control negativo; ambas devolvieron código uno.

Evidencia de los fallos inducidos:
`A:\nsc-interop-build\tanda08-atomic-global-matrix-validation-7467-1402` y
`A:\nsc-interop-build\tanda08-atomic-global-extra-validation-7474-22899`.
Esta validación no certifica los demás scripts ni cierra la tanda.
