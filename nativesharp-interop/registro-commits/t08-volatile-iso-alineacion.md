# Tanda 8: alineación del acceso volatile ISO

La primera matriz ISO identificó que NSC emitía accesos `i64` con alineación 4 cuando la referencia Clang usaba 8: `A:\nsc-interop-build\tanda08-volatile-pointer-22917-28944`. Se separa la alineación demostrada del orden de memoria: la ruta directa conserva la alineación tanto en modo ISO como Microsoft, y solo aplica acquire/release en modo Microsoft. Se ajustan lectura, escritura y actualización compuesta de esa ruta.

La construcción y la repetición de la matriz están pendientes en el momento de este commit. No se afirma soporte para punteros que pasan por locales o parámetros.
