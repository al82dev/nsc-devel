# T08-015: dirección interna para coalescencia volatile

El proyecto mixto terminó con código 1 porque `??=` exige `addressable`,
mientras el identificador del global C volatile lo mantiene falso para evitar
que su dirección escape y pierda el calificador. La dirección de `??=` es
interna y no se devuelve como puntero: se permite exclusivamente el caso de
identificador modificable volatile con tipo puntero y apuntado sin calificar.
No se altera `addressable` ni se habilita `&`. Falta repetir el grupo de
pruebas y confirmar las rutas positiva y negativa.
