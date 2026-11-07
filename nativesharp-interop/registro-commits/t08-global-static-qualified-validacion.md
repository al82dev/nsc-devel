# T08-004: validacion de globales C internas calificadas

La construccion incremental termino correctamente. El control C y NSC pasaron
O0–O3 para const unsigned int, volatile short, puntero a const int, puntero
const a int y puntero a volatile int. Tres escrituras prohibidas produjeron
errores; se compararon load acquire y store release volatile de 16 y 32 bits
en el IR de Clang y NSC. Pasaron las regresiones de identidad, agregados,
TLS y ambiguedad.

Se agrega el control al gate de la tanda, aun no ejecutado por completo. Estos
casos no acreditan todas las combinaciones de calificadores ni tipos TLS.
