# T08: operaciones compuestas sobre globales C atómicos no TLS

La ejecución de `pruebas-tanda-08-atomic-global-matrix/validar.cmd` terminó con
código 0. En O0–O3 se comprobaron las operaciones compuestas de cinco enteros,
dos flotantes y un puntero, seguidas por observación desde C. El IR de Clang y
NSC mostró actualizaciones atómicas `seq_cst` en los siete valores numéricos;
el IR de NSC también mostró actualización atómica del puntero.

Clang rechazó la expresión de puntero atómico en C11; por ello esa operación
no se presenta como una comparación de expresiones idénticas entre lenguajes.
La prueba no acredita todos los operadores, otros tipos ni concurrencia. El gate
agrupado de la tanda aún no se ha repetido; T08-004 y la tanda siguen abiertos.
