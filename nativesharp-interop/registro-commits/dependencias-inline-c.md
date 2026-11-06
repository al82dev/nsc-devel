# Dependencias internas de inline C

La emisión de Clang registra las definiciones internas del encabezado antes de solicitar las funciones seleccionadas. Esto permite que un cuerpo inline use helpers o variables estáticas sin convertirlos en símbolos públicos; la ejecución y el estado compartido se comprueban mediante fixtures.
