# Tanda 8: arreglos de punteros C volatile

Se amplía la matriz con arreglos de punteros a `_Bool`, enteros de varios anchos, `float` y `double`, usando tanto índice como desreferencia. Se exige el orden y alineación de Clang para cada ancho, además de la ejecución O0–O3.

La línea base falla al comparar la cantidad de lecturas `i8 acquire`; el IR muestra accesos volatile sin acquire/release para los arreglos. Evidencia: `A:\nsc-interop-build\tanda08-volatile-pointer-20393-10730`. La corrección se registrará por separado.
