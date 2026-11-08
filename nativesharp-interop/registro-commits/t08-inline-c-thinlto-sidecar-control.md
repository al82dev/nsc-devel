# T08-003: control de sidecars en el fallback ThinLTO

El control crea sidecars antiguos antes de producir cada biblioteca y exige
que se retiren. En O1–O3 comprueba además que el ejecutable directo informe un
índice ThinLTO y que el consumidor no sustituya las bibliotecas por sidecars.
Las comprobaciones son adicionales a ejecutar la matriz de estado independiente.
Su resultado se registrará tras correr el control actualizado.
