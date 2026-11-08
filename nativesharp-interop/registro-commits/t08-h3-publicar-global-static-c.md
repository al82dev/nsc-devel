# T08: global C static en plantilla precompilada

La reproducción `ca7461d23` fallaba porque el catálogo omitía las globales internas mencionadas por plantillas públicas. Su implementación tampoco se solicitaba para el objeto Clang cuando solo aparecía en una plantilla.

La captura incorpora las candidatas internas que se resuelven a una global C, conservando el símbolo de almacenamiento generado. El verificador admite esa forma de identidad sin tratarla como importación DLL. La generación agrupa la global y las funciones inline solicitadas de la misma sesión Clang, para que compartan el almacenamiento.

La reproducción pasó en O0–O3: la plantilla incrementa la global y una función C devuelve el mismo valor. El conjunto dirigido de cinco dependencias C pasó en los cuatro niveles. Esto no acredita todas las formas de enlace interno ni cierra la tanda 8.
