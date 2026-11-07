# T08: operaciones compuestas en globales atómicos C no TLS

Se prepara una comprobación agrupada de operaciones compuestas de enteros,
flotantes y puntero con acceso cruzado C/NativeSharp en O0–O3. El IR de Clang y
NSC debe contener actualización atómica `seq_cst` para cada global. Este cambio
no se ha ejecutado y no acredita el soporte; no modifica producción ni NSLink.
