# T08-004: validacion de ambiguedad con tipo no proyectado

La construccion incremental termino correctamente. El control de dos globales
internas homonimas `int` e `int**`, antes aceptado, ahora obtiene el diagnostico
de origen ambiguo. Las regresiones de identidad entre productores y de tipos
internos pasaron O0–O3. Se incorpora el control al gate de la tanda, no ejecutado
por completo; T08-004 sigue abierto.
