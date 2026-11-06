# Identidad opaca C con encabezado en otra ruta

Se prepara un consumidor que usa una copia del header, con el mismo contenido
pero distinta ruta, para intentar cruzar punteros con el productor. Debe
rechazarse la conciliación sin evidencia de que sea el mismo archivo. El
fixture copia el header solo dentro de A: y conserva el original. Pendiente de
validación con el conjunto de procedencia.

Se añade el control del mismo archivo de salida modificado después de construir
el productor. El fixture restaura esa copia al comenzar y preserva el header
fuente. Falta ejecutar el control y confirmar el diagnóstico.
