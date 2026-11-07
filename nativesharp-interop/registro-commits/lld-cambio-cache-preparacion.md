# Preparación del control de cambio de enlazador

El fixture construye un programa NativeSharp con NSLink y enlace incremental,
lo vuelve a construir con LLD en la misma salida y exige `miss`; un tercer
enlace con LLD debe producir `hit`. También prueba la selección de LLD con un
PATH local donde no está disponible y exige error sin ejecutable residual.
La configuración del entorno solo afecta al proceso de la prueba. Esta
matriz pasó con NSC build `b910fa13ea6d5855`: el cambio produjo `miss`, la
repetición `hit`, y la herramienta ausente produjo diagnóstico. La variante
con una caché previa también falló correctamente y conservó el ejecutable
existente byte por byte. Se agregó el control al validador de tanda.
