# Proyección de arreglos TLS volatile escalares

El control C pasó y NSC rechazó cuatro arreglos escalares `volatile`. Se
permite que los elementos escalares lleguen al mapeo C existente, que valida
tipos y dimensiones y conserva `volatile`. Records y enums volatile mantienen
la barrera previa. Pendiente la construcción y validación agrupadas.
