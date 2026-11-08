# Conciliación de enum C independiente

El consumidor con header y metadata rechazó `NMood`: la conciliación anterior
solo registraba enums que formaban parte del cierre de campos de un record.
Se prepara la comparación de enums nombrados visibles por contrato serializado
completo y USR conocido, aceptando una única coincidencia. El mecanismo usa la
misma codificación que el contraste de enums dentro de records y no modifica
el formato persistente. Construcción y pruebas agrupadas quedan pendientes.
