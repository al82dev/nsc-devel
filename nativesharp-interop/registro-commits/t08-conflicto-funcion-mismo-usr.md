# Conflicto de funciones con el mismo USR

T08-002/T08-003: antes de agrupar por símbolo externo, el inventario compara
declaraciones externas del mismo proyecto con el mismo USR conocido. Si difieren
en símbolo, triple, tipo canónico, convención, linkage o dllimport informa un
conflicto. El USR no sustituye las comprobaciones existentes ni impide que
declaraciones con USR distintos sean alias compatibles del mismo símbolo.
Las pruebas y la validación agrupada están pendientes.
