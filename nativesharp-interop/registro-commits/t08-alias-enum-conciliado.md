# Alias de enum C conciliado

Tras preparar la conciliación de enums independientes, el control avanzó hasta
la resolución de `NMood`, que rechazó las observaciones por dueño diferente.
Se amplía el guard de alias conciliados para admitir un enum únicamente con
USR coincidente y una identidad viva vinculada a la precompilada. Nombre,
tipo canónico y triple continúan comprobándose. Faltan construcción y
validación agrupadas, incluidos los rechazos de contratos incompatibles.
