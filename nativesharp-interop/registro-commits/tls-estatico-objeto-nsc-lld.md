# Referencia TLS con objeto NativeSharp y LLD

Se preparó una matriz reproducible O0–O3 que genera el objeto NativeSharp,
lo enlaza con la biblioteca C TLS mediante LLD y ejecuta el mismo control de
aislamiento entre hilos. Un punto de entrada auxiliar invoca el símbolo Main
del objeto; se usa solo como instrumento y no sustituye la validación del
arranque y enlace normales de NSC. La matriz aún está pendiente de ejecución.
