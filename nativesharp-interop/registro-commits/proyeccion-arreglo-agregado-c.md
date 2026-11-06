# Proyección de puntero C a arreglo de struct o enum

Las llamadas a funciones con `CPointArray*` y `CColorArray*` se rechazaron
como identificadores desconocidos. Se prepara la proyección de su elemento
nominal usando el tipo importado, USR y layout comprobados, conservando
dimensiones y const. Se mantienen los rechazos de volatile y punteros como
elementos. La conversión de enumeradores es un problema independiente.
Pendiente de construcción y validación de las llamadas y sus barreras.

La construcción incremental `42c0d4185eae615e` pasó la suite de sesión 1/1.
Los proyectos reales de arreglos de struct y enum ejecutaron O0–O3, con
lectura y mutación desde C. Los argumentos const y de identidad diferente se
rechazaron. La inicialización del enum se hizo desde C por el fallo independiente
de asignar directamente un enumerador C; otras formas y volatile siguen abiertas.
