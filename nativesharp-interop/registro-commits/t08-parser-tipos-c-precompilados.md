# T08: nombres C precompilados durante el analisis sintactico

El consumidor con solo metadata no reconocia `VMode` en un cast, aunque el enum C se restauraba mas adelante durante la resolucion de tipos. El analizador necesita conocer el nombre antes para distinguir el cast de una expresion.

El driver instala un proveedor de tipos C precompilados para el analizador. El proveedor reutiliza la reconstruccion del catalogo existente y agrega esos tipos al inventario sintactico por proyecto, tanto al analizar las unidades como al clonarlas para observacion. La seleccion semantica sigue siendo independiente y el formato NSMETA no cambia.

La compilacion incremental termino con exito. `pruebas-tanda-08-volatile-campos-c/validar.cmd` termino con codigo 0: ejecuciones directa, encabezado mas biblioteca, y solo metadata en O0-O3; comparacion de accesos con Clang aprobada. El control informa adicionalmente que `ProbeRecord` y `ProbeOddConsumer` siguen rechazados; esos diagnosticos no hacen fallar el script y no se consideran resueltos. La cobertura negativa de visibilidad entre proyectos y la validacion completa de tanda 8 siguen pendientes.
