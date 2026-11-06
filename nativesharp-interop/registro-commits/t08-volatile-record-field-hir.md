# Propagación HIR de volatile a campos

Los campos conservan `volatile` de la dirección del record receptor al generar
HIR, además del calificador propio del campo. La propagación se limita a nodos
de dirección para no calificar el objeto apuntado por un puntero volatile.
Falta comprobar cargas, escrituras y regresiones en la batería agrupada.
