# Vista de un parámetro move-only por valor

Un parámetro `Rc` o `Arc` recibido por valor podía reemplazarse mientras una
vista derivada de él seguía viva; también se aceptó mover el parámetro `Rc`.
Se observaron los resultados del análisis semántico antes del cambio, sin
ejecutar el acceso posiblemente inválido. La salida anterior no se conservó
en un log independiente.

El seguimiento de raíz de una vista obtenida de un método de instancia solo
reconocía locales. Ahora también identifica un parámetro por valor cuyo tipo
es move-only. La consulta de préstamos activos resuelve esa raíz en el ámbito
de la vista y compara el ordinal del parámetro; el destino de asignación consulta
esa misma protección antes de reemplazar el parámetro. Una vista que depende
del parámetro por valor tampoco puede devolverse después de su destrucción.

Tras compilar `nsc` incrementalmente, la regresión dirigida con controles
internos locales y atómicos rechazó el reemplazo y el movimiento con vista viva,
y ejecutó el reemplazo después de finalizar el ámbito de la vista en O0/O2.
Desde fuentes, las pruebas públicas hicieron lo mismo para `Rc` y `Arc`,
rechazaron la devolución de la vista y el reemplazo con una vista asignada
después de declararse, y ejecutaron lectura, fin de ámbito y reemplazo de otro
parámetro independiente en O0/O2. No se prueba aquí toda forma de receptor,
sombra de nombres, consumo binario ni concurrencia; el seguimiento existente
de raíces sigue empleando nombres y ámbitos.
