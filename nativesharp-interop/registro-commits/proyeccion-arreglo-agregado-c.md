# Proyección de puntero C a arreglo de struct o enum

Las llamadas a funciones con `CPointArray*` y `CColorArray*` se rechazaron
como identificadores desconocidos. Se prepara la proyección de su elemento
nominal usando el tipo importado, USR y layout comprobados, conservando
dimensiones y const. Se mantienen los rechazos de volatile y punteros como
elementos. La conversión de enumeradores es un problema independiente.
Pendiente de construcción y validación de las llamadas y sus barreras.
