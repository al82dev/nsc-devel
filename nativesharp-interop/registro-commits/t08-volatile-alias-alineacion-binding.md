# T08-010/015: dato de alineación en el binding de expresión

El binding semántico conserva la alineación conocida que ya calcula el análisis
de expresiones. El valor por defecto es uno, por lo que la ausencia de una
prueba no presupone alineación natural.

El dato todavía no habilita accesos atómicos: antes hay que comprobar y, si
corresponde, corregir su actualización en reasignaciones y uniones de flujo.
No se ha construido ni ejecutado este cambio; se validará con el conjunto.
