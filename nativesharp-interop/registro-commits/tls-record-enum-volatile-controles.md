# Controles de records y enum TLS volatile

El fixture exige carga y escritura `volatile` por global en el IR, y carga
para el record `const volatile`. Agrega el rechazo de asignación a su campo en
estático y DLL. Pendiente construcción y validación agrupadas.
