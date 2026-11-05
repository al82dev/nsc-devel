# Acceso concurrente a sesiones

Se agrega una prueba con cuatro hilos que consultan una misma sesión y comparan
dos sesiones en órdenes opuestos. El propietario mantiene vivas ambas sesiones y
las cierra después de unir los hilos.

Esta prueba se ejecutará con el conjunto de la tanda; no sustituye una herramienta
de detección de carreras ni prueba la destrucción simultánea del objeto sesión.
