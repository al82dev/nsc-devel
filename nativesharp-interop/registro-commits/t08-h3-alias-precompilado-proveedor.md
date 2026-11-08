# Tanda 8: proveedor de alias C precompilados

Las plantillas públicas pueden necesitar un typedef C al analizar su código en
el consumidor. El proveedor combina los alias de sesiones Clang existentes con
los del catálogo precompilado, antes de analizar los archivos. Comprueba que el
módulo propietario esté presente y sea inequívoco, conserva la identidad de la
referencia y deja la salida sin modificar si la reconstrucción falla. El driver
lo configura cuando hay un catálogo C precompilado.

Se construyeron incrementalmente `nsc_driver` y `nsc_cpp_c_identity_tests`;
la prueba dirigida pasó. Esta usa un catálogo interno sintético que contiene
un alias real de Clang. No demuestra la publicación de dependencias desde una
biblioteca real ni la ejecución de una plantilla consumidora. La generación
del catálogo de dependencias, la reconciliación entre orígenes y la validación
completa de la tanda siguen pendientes. La compilación mostró advertencias de
inicializadores en código existente.
