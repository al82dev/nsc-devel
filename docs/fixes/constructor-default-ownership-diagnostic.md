# Diagnóstico de ownership con parámetros predeterminados

LKF-288: cuando un argumento de constructor se omitía por tener valor predeterminado,
la explicación de un rechazo de ownership devolvía el mensaje genérico de sobrecarga.
Esa ruta exigía que el número de parámetros coincidiera con el de argumentos explícitos,
aunque ya disponía del mapa validado de argumentos a parámetros.

La explicación ahora acepta el mapa válido con parámetros predeterminados omitidos. La
selección y la aceptación de candidatos no cambian. La prueba dirigida cubre el rechazo
específico y el control válido con transferencia explícita; pasó 82 comprobaciones.
El compilador de la copia también confirmó el mensaje para el argumento omitido y para
el argumento explícito, y aceptó `move(a)` con el valor predeterminado. Permanecen fuera
de este cambio otras rutas y variantes de LKF-288.
