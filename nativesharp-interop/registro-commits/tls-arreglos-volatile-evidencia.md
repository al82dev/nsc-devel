# Evidencia de arreglos TLS volatile

Con NSC `a83d41a567fa4257`, la construcción incremental y la suite de sesiones
pasaron. Los controles C y NativeSharp para arreglos TLS `volatile` de
`unsigned int`, `long long`, `double` y `_Bool` pasaron O0–O3 con biblioteca
estática y DLL, incluida la comprobación de aislamiento entre hilos. El IR
estático contiene carga y escritura `volatile` para cada arreglo.

Pasaron también las regresiones dirigidas de punteros y escalares TLS, los
controles de `const` y el rechazo de puntero anidado no proyectado. Persisten
otros contratos, los controles de DLL mínima y el gate integral de la tanda.
No se modificó NSLink ni se desplegó en C:.
