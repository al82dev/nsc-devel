# Identidad opaca C con encabezado en otra ruta

Se prepara un consumidor que usa una copia del header, con el mismo contenido
pero distinta ruta, para intentar cruzar punteros con el productor. Debe
rechazarse la conciliación sin evidencia de que sea el mismo archivo. El
fixture copia el header solo dentro de A: y conserva el original. Pendiente de
validación con el conjunto de procedencia.
