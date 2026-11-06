# Identidad de record en proyección de firmas

El índice temporal de records conserva el grupo completo durante la proyección
de funciones y métodos. Así comprueba el USR del tipo referido incluso cuando no
hay ProjectNameLookup, sin reemplazar los controles de estado y duplicados.
Las referencias al grupo viven solo durante la llamada. Prueba y validación
agrupada pendientes.
