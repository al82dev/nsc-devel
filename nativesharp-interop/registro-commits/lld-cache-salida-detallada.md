# Salida detallada del control de caché

La primera ejecución del fixture terminó las tres compilaciones, pero la
consola resumida de NSC ocultó los campos `incremental-link`, por lo que el
control no pudo verificar `miss` ni `hit`. Se solicita `--verbose` para
capturar esos campos; las comprobaciones no se debilitan. La nueva ejecución
queda pendiente.
