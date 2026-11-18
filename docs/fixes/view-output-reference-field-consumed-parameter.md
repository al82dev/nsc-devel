# Salida mediante campo ref desde parámetro consumido

En `0d7df1812`, un método podía guardar por un campo ref una vista de un
parámetro move-only recibido por valor, aunque ese parámetro deja de vivir al
retornar. La comprobación semántica aceptó la reproducción conservada en
`A:/nsc-fixes-evidencia/view-ref-field-parameter-before-20261112.log`; el
programa potencialmente inválido no se ejecutó.

La guarda conservadora de `recordViewOutputOrigins` reconoce ahora también
parámetros move-only por valor, con el mismo criterio que se usa para salidas
ref/out. No cambia la regla para parámetros recibidos por referencia.

El target incremental `nsc` terminó 0, SHA-256
`670b1aa0d2c097756e88d3335a2892d1baa9c10d8d97245a67082ca769a16e9f`.
El nuevo caso fue rechazado con el diagnóstico esperado y pasaron los controles
de campos ref y alias de parámetro. Otra prueba conservó el caso válido de
parámetro por referencia y rechazó el escape en su llamador. Los logs
`view-ref-field-parameter-after-20261112.log`,
`view-ref-field-parameter-controls-20261112.log`,
`view-ref-field-parameter-alias-controls-20261112.log` y
`view-ref-field-borrowed-param-controls-20261112.log` están en
`A:/nsc-fixes-evidencia`. Se trata de análisis semántico dirigido, no de
ejecución nativa ni prueba exhaustiva de todas las formas de alias.
