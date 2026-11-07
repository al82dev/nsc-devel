# T08: orden de acceso al apuntado volatile float

El IR de NSC para `more_float_volatile[1]` mostró carga y escritura volatile
ordinarias. Clang 23, para la misma ruta C de apuntado volatile float con
alineación cuatro en modo MS, emitió carga acquire y escritura release.
`clangMsVolatileDirectCPointeeAlignment` reconocía short, enteros y double,
pero no float, aun cuando HIR transportaba el marcador y la alineación.

Se agrega únicamente el caso float cuyo árbol procede de un global C de
puntero atómico `float*`, con el marcador de semántica MS y alineación existente.
El cambio aún no se ha construido ni comprobado; faltan IR, ejecución y
regresiones. No se modifica NSLink ni metadata.
