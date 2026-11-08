# T08 H3: ámbito de parámetros de lambda

El AST de una lambda guarda su cuerpo en `first` y sus parámetros como hijos,
igual que los métodos analizados por el filtro. Se extiende la comprobación de
sombras a sus parámetros únicamente dentro de ese cuerpo; las expresiones fuera
de él mantienen sus propias demandas de importación C.

El control de integración acompaña el cambio; su resultado se registra por
separado. Esto no cierra todos los casos de ámbito de la tanda 8.
