# Conciliación de identidad de alias

Al resolver un alias importado, el frontend conserva los USR conocidos de
las observaciones que compiten por el mismo nombre. Un USR ausente o vacío no
borra uno anterior y los USR conocidos contradictorios producen ambigüedad.
Esto no acredita persistencia ni completa la cobertura de T08-002; la prueba
agrupada de la tanda queda pendiente.
