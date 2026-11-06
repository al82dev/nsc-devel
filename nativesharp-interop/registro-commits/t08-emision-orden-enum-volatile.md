# T08-015: emisión de orden para enum volatile alineado

El emisor aplica acquire en lectura y release en escritura simple o compuesta
cuando el helper acredita modo, enum y alineación. Los accesos sin garantía
conservan su emisión anterior. La construcción y la comparación con Clang
siguen pendientes; no se afirma corregido hasta ejecutar la validación.
