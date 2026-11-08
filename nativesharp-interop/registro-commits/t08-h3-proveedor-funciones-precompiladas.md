# Tanda 8: proveedor preparado de funciones C precompiladas

El proveedor combina funciones de sesiones Clang con funciones demandadas de
catálogos precompilados. Comprueba el módulo propietario, reproyecta la firma
con las reglas existentes y detecta conflictos de símbolo e identidad antes de
entregar el resultado. Solo instala funciones solicitadas por el consumidor.

La compilación incremental y `nsc_cpp_c_identity_tests` terminaron con código
0. El fixture restauró `cid_function` de un catálogo sintético con un módulo
precompilado en memoria. El driver aún no configura este proveedor, la
metadata normal aún no publica funciones y el verificador todavía las rechaza.
Faltan pruebas de enlace, ejecución, conflictos, enlace interno y demás
contratos; este cambio no cierra la tanda 8.
