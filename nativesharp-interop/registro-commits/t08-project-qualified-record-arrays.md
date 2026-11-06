# Arreglos C globales de records calificados

La proyección reutiliza la verificación de identidad y layout de records C
para arreglos const o volatile y conserva los calificadores del arreglo y del
elemento. No habilita punteros calificados ni el paso de records por valor.
La propagación de acceso y el resultado funcional requieren validación conjunta.
