# Diagnóstico de ownership con argumentos nombrados de constructor

LKF-288: el análisis ya conservaba el parámetro asignado a cada argumento nombrado, pero al
explicar el rechazo usaba la posición del argumento como índice de parámetro. Una llamada
reordenada podía atribuir al parámetro equivocado la necesidad de transferir propiedad.

El mensaje toma ahora el nombre y el tipo mediante el mapa del candidato evaluado. Si el mapa
no está completo o apunta fuera de los parámetros, se conserva el diagnóstico genérico.
No cambia la selección de sobrecargas, la propiedad ni la posición del argumento reportada.

La reproducción `Holder(second: b, first: a)` antes señalaba incorrectamente `second` y ahora
señala `first`; el control posicional conserva su diagnóstico y `first: move(a)` se acepta.
El compilador de la copia confirmó los tres casos. La prueba incremental
`nsc_project_construction_model_tests` pasó 80 comprobaciones. Otras rutas de diagnóstico de
LKF-288 permanecen fuera de esta corrección.
