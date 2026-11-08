# Tanda 8: verificación del catálogo preparado de alias C

El verificador de cierre binario reconoce el catálogo de dependencias preparado
para metadata 46, comprueba su codificación canónica y exige identidad Clang
para los alias. Por ahora rechaza funciones y globales en ese catálogo: sus
reglas de restauración y verificación todavía no están conectadas. La emisión
normal continúa deshabilitada.

La prueba dirigida verificó la aceptación del catálogo de alias obtenido de un
proyecto con referencia C y el rechazo de un alias sin USR. La compilación
incremental y `nsc_cpp_c_identity_tests` terminaron con código 0. Sigue sin
probarse la serialización y lectura de un NSMETA real por el driver, la
ejecución del consumidor y las colisiones de nombres.
