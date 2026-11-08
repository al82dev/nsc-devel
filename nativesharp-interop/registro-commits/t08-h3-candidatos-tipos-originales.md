# T08 H3: conservar nombres originales de tipos en plantillas

El registro de candidatos conserva la escritura original de cada tipo antes de
que la búsqueda de nombres pueda expandir un typedef en el AST. El recorrido
incluye argumentos de tipo y tipos de retorno y error de funciones anidadas;
mantiene el mismo contexto léxico de la plantilla. El control existente ahora
consulta la escritura conservada de `CIdInt`.

Esto no selecciona un alias ni prueba que pueda restaurarse desde NSMETA.
La construcción incremental y el control conjunto con el selector de alias
terminaron con código 0. El control acredita la escritura original de `CIdInt`;
no prueba por separado todos los tipos anidados. Quedan pendientes la selección
semántica del alias y el consumo precompilado. La tanda 8 no se cierra.
