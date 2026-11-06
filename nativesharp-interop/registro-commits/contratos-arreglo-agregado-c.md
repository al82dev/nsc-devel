# Contratos de arreglos C de struct y enum

Se inicializa el arreglo enum desde C para aislar un fallo independiente en la
conversión de enumeradores. Se preparan controles negativos para const y para
identidades de elemento distintas. Pendiente ejecutar y validar.

Con `42c0d4185eae615e` pasaron las matrices O0–O3 y los tres casos negativos
se rechazaron por la llamada esperada. Se verificaron sus diagnósticos concretos.
