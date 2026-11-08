# Validación de selección del enum conciliado

La construcción incremental de `88b6c7fdf` pasó con hash
`c2f36ec37b2775bf` y tres advertencias de inicializadores en
`CppSemanticRecord.cpp`. `validar-referenciados.cmd` terminó con código 0:
el consumidor con el mismo encabezado y el consumidor con encabezado oculto
compilaron y ejecutaron de O0 a O3. El control negativo rechazó `NAlias` con
tipos `int` y `struct NRecord` distintos. La regresión de miembros anónimos,
incluida metadata, terminó con código 0 de O0 a O3.

La comparación de enum independiente exige USR y contrato serializado
coincidentes; no demuestra procedencia física independiente del enum. Las
pruebas son dirigidas y no cierran los ítems ni la tanda 8.
