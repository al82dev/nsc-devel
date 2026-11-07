# Evidencia de arreglos TLS escalares

Con NSC `df824a793de82e55`, la construcción incremental y la suite de sesión
pasaron. Los controles C y NativeSharp de arreglos TLS `unsigned int`,
`double`, `_Bool`, `const int` y `volatile int` pasaron O0–O3 en estático y
DLL, con lectura, escritura cuando corresponde y aislamiento de otro hilo.
La prueba comprueba el rechazo de escritura `const` en ambos enlaces y
confirma en el IR estático las operaciones `volatile`.

Las regresiones de puntero y global TLS estáticos pasaron. Otros tipos y
calificadores, las variantes de DLL mínima y el gate integral siguen abiertos.
No se modificó NSLink ni se desplegó en C:.
