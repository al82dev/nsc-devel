# Tanda 08: acceso a campos de records C

En una unidad C los campos de `struct` y `union` no tienen control de acceso
C++. La captura los representa como públicos para el modelo de interoperación;
en C++ se conserva el acceso proporcionado por Clang. El cambio no permite aún
los campos de union ni los campos de bits y espera la validación de la tanda.
