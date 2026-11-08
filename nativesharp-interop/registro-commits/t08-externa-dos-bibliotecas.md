# Global C externa compartida entre dos bibliotecas

Dos productores incluyen una declaración `extern int internal_value` e importan
la misma biblioteca C, que define la variable con valor inicial 11. Sus plantillas
leen y modifican esa variable. El consumidor espera que ambas vean el mismo valor.

Los productores se compilan en O0. Al compilar el consumidor se obtiene
`ambiguous namespace state reference 'internal_value'` en ambas plantillas.
La prueba `externa-dos-bibliotecas/validar.cmd` conserva el caso y debe pasar
cuando la conciliación de globales esté corregida.

La selección de redeclaraciones en `selectCppSemanticGlobal` compara símbolo,
USR y tipo dentro de cada proyecto; la restauración conserva propietarios
distintos. Falta demostrar la regla adecuada entre proyectos sin unir tipos,
calificadores o enlaces incompatibles. No se modificó la resolución compartida.
