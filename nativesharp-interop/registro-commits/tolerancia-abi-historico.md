# Lectura y compatibilidad de metadata histórica

La referencia archivada 1.37 puede leerse, pero declara Object ABI 2. El
compilador actual requiere Object ABI 3 y la rechaza con un diagnóstico de
recompilación. La prueba comprueba ambas cosas por separado y conserva los
controles de tolerancia para datos compatibles. Falta validación agrupada.
