# Alineación del almacenamiento de puntero global volatile

T08-015: un global importado cuyo almacenamiento es un puntero volatile de
64 bits puede conservar la alineación acreditada por Clang cuando la opción MS
está activa. No se modifica la calificación del objeto apuntado. La emisión y
los controles de IR se registran por separado; falta validación agrupada.
