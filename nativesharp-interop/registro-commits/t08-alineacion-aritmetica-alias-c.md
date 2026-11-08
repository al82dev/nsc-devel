# T08-010/015: alineación de aritmética de alias C volátil

El diagnóstico `t08-volatile-alias-aritmetica-diagnostico.md` muestra que
`*(data + index)` pierde el orden esperado en modo Microsoft. El analizador
conservaba el contrato C del alias pero reducía siempre su alineación a 1.
Cuando el alias C volátil apunta a un escalar de tamaño conocido, ahora conserva
el máximo común divisor entre la alineación probada de entrada y el tamaño del
elemento. La alineación desconocida, los otros tipos y punteros nativos siguen
con garantía 1. No se deduce la alineación inicial del tipo del puntero.

La construcción incremental de NSC y de ambos controles terminó con código 0.
Los controles de modelo y HIR pasaron 78, 96, 40 y 8 comprobaciones. La prueba
de cuerpos pasó 119 de 120 comprobaciones: falló la misma comprobación previa
de topología de ondas paralelas, sin fallos reportados en las nuevas de
alineación. El modo por consumidor continúa protegido. Este cambio no resuelve
por sí solo el diagnóstico ni cierra la tanda 8; falta la validación completa.
