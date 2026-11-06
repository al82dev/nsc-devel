# T08-010: dirección de campo importado desalineado

La dirección de un campo C escalar con alineación inferior a la natural no
debe convertirse todavía en un puntero ordinario cuyo acceso posterior pueda
suponer alineación natural. El análisis del acceso de miembro mantiene
asignable el campo pero impide exponer su dirección o pasarlo por referencia.
El proyector todavía rechaza estos campos hasta el siguiente cambio; las
pruebas de integración quedan pendientes de la ejecución agrupada.
