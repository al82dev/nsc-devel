# Emisión del acceso directo a global C volatile int*

La carga o dirección HIR marcada para el caso directo se traduce a acquire
en lectura y release en escritura con alineación cuatro. La asignación
compuesta aplica ambos órdenes a sus operaciones respectivas. Los nodos sin
la marca específica no cambian de semántica. Falta la validación agrupada.
