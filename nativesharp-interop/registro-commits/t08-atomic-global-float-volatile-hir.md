# T08: HIR del apuntado volatile float en puntero atómico C

La proyección preparada de `_Atomic(volatile float*)` necesita propagar la
semántica volatile de Microsoft a los accesos directos e indexados del
apuntado. Se agrega `float*` a esas dos rutas HIR, usando el ancho de cuatro
bytes ya calculado por la ruta común. El cambio no se ha construido ni probado;
no acredita órdenes de memoria, escapes ni otros tipos. NSLink no se modifica.
