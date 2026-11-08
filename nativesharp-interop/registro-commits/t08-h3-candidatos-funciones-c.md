# Tanda 8: captura preparada de funciones C de plantillas

La captura conserva observaciones proyectadas de funciones C externas que
coinciden con demandas de plantillas públicas, incluyendo módulo, referencia,
USR y símbolo. Evita duplicar la misma observación. Los conflictos diferidos,
declaraciones no proyectadas y funciones de enlace interno todavía no están
representados por esta captura; no deben considerarse resueltos.

La compilación incremental y `nsc_cpp_c_identity_tests` terminaron con código 0.
La prueba incluye una plantilla y referencia C reales y comprueba el símbolo
`cid_function`. El productor aún no publica estas funciones, el verificador del
catálogo aún las rechaza y el consumidor no las restaura. La prueba no acredita
enlace, ejecución, overloads ni cierre de la tanda.
