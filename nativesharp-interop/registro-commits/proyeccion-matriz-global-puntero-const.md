# Proyección de matriz global C de punteros a const

El fixture `const int *c_matrix[2][2]` falló en O0 porque el inventario
rechazaba arreglos de más de una dimensión. Se prepara el recorrido de las
dimensiones hasta el puntero simple y la conservación del const apuntado.
Solo aplica a arreglos C de dimensiones fijas y elementos escalares admitidos.
La indexación aún debe transportar el calificador; falta validar el conjunto.
