# Tanda 08: conflicto diferido de tipos enum C

Las observaciones incompatibles del mismo enum se conservan como un candidato
con diagnóstico diferido, en lugar de fallar al recolectar headers no usados.
El enlace de nombres informa el conflicto al usar el tipo. La selección desde
el driver, los usos en expresiones y las pruebas completas siguen pendientes;
el cambio no se ha compilado ni ejecutado.
