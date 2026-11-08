# Tanda 8: captura preparada de globales C de plantillas

La captura consulta las demandas de plantillas públicas, utiliza el proveedor
de globales existente y conserva la declaración C externa proyectada con su
módulo, referencia, USR y símbolo. TLS, enlace interno, conflictos y tipos que
no se puedan proyectar no quedan cubiertos por esta captura.

La compilación incremental y `nsc_cpp_c_identity_tests` terminaron con código
0. El fixture comprueba la captura de `cid_global` desde una referencia C real.
El productor todavía no publica estas globales, el verificador las rechaza y
el consumidor no las restaura. Quedan pendientes enlace, ejecución y validación
completa de la tanda.
