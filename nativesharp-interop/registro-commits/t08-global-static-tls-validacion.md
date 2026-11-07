# T08-004: validacion de global C estatica TLS int

La construccion incremental termino correctamente. La referencia C paso en el
hilo principal; NativeSharp accedio a la misma instancia que las funciones
inline y un segundo hilo observo su valor TLS inicial, en O0, O1, O2 y O3.
El hilo principal conservo su valor tras esperar al segundo hilo.

Pasaron las regresiones de identidad y agregados estaticos y la regresion TLS
atomica. Se agrega el control al gate de la tanda, no ejecutado por completo
con estos cambios. No se acreditan otros tipos TLS ni solapamiento simultaneo.
