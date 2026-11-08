# Comparación de IR volatile para agregados anónimos C

`pruebas-tanda-08-anonimos-c/validar.cmd` terminó con código 0. El comparador
contrastó getters y setters del `int` natural y empaquetado contra Clang 23:
los cuatro accesos coincidieron en orden, volatilidad, tipo y alineación en cada
uno de los 12 reportes (Project, Repeat, Shared y Metadata; O1–O3). No hubo
resultados FALTA, DEFECTO ni SUBALINEADO. Los programas de ejecución también
pasaron O0–O3.

Esta evidencia cubre los dos campos concretos del fixture, no todos los tipos ni
formas de volatile. La validación completa de la tanda 8 sigue pendiente.
