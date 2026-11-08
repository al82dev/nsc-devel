# Firmas de función C externa entre encabezados

El control dirigido usa dos encabezados que declaran `clash(int)` y verifica
que NSC acepta la llamada. La variante sustituye la segunda declaración por
`clash(double)` con otro retorno y comprueba que NSC diagnostica declaraciones
incompatibles para el mismo USR. `funcion-firma/validar.cmd` pasó en análisis
semántico con el compilador de desarrollo y Clang en A:.

Es una muestra dirigida; no incluye enlace, ejecución ni todas las diferencias
de firma posibles. La validación completa del bloque sigue pendiente.
