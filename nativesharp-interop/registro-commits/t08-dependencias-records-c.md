# Tanda 08: dependencias transitivas de records C

La seleccion de tipos preparados recorre campos de records C seleccionados y
agrega records referidos a traves de punteros y arreglos hasta punto fijo.
Conserva las barreras para colisiones y definiciones no verificadas. Se requiere
probar ciclos, visibilidad, acceso y layout en la validacion agrupada.
