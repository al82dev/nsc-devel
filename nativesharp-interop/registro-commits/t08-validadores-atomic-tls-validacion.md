# T08: validación de propagación en controles atómicos y TLS

Los seis controles de globales atómicas y TLS pasaron sus rutas normales; los
dos precompilados pasaron de nuevo O0–O3 después de corregir su restauración.
Las rutas normales de la matriz atómica y globales estáticas también pasaron.

Tres copias aisladas reprodujeron y rechazaron: un ejecutable que devolvió 42,
un fallo de DLL en el flujo precompilado global y otro en el flujo TLS. Ambos
encabezados ocultos quedaron restaurados tras las fallas. Evidencia:
`A:\nsc-interop-build\tanda08-atomic-global-validation-9087-24195`,
`A:\nsc-interop-build\tanda08-atomic-global-precompiled-9557-31870` y
`A:\nsc-interop-build\tanda08-atomic-precompiled-9796-30072`.

Durante la comprobación se detectó una colisión entre nombres de etiquetas del
script TLS; separar los nombres y repetir el fallo devolvió uno. El resto de
validadores de la tanda requiere auditoría: este registro no certifica el gate
completo ni cierra T08-004.
