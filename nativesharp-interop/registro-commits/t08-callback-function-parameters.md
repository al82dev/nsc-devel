# Tanda 08: punteros a función en firmas C

La proyección convierte punteros a función con firma escalar representable a
la identidad estructural usada por NSC, conservando convención y variadicidad.
Se rechaza const del apuntado en la firma hasta que su identidad lo represente.
La invocación de callbacks, tipos compuestos y ABI aún requieren validación.
