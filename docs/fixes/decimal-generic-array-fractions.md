# Matriz genérica de decimales: controles adicionales

Se amplió el fixture de regresión existente para comprobar fracciones,
signos, escalas y sustitución de una celda sin alterar otra. La matriz del
ejemplo original permanece intacta. El ejecutor dirigido construye y ejecuta
la prueba en O0 y O2.

La ejecución integrada de `decimal-matrix-integrated-20261111.cmd` terminó con
código 0 y generó los dos ejecutables en
`A:/nsc-fixes-evidencia/decimal-matrix-integrated-20261111`. El comando pasó
`nslink.exe` por nombre; el PATH lo resuelve a una dependencia externa en C,
usada solo para lectura. El compilador y runtime usados están en A. Esta
comprobación cubre esos valores y operaciones, no todas las posibles escalas
o combinaciones. El árbol contenía otros cambios locales y no certifica una
revisión limpia.
