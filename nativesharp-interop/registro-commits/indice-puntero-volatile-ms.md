# Índice directo de global C volatile int*

El HIR marca el elemento indexado cuando su receptor es el global C
`volatile int*` y el modo MS está presente. La carga conserva esa marca y el
lowering reconoce el nodo de dirección del índice para aplicar los órdenes
ya usados por la desreferencia directa. Esta ruta no se activa para campos,
locales ni punteros arbitrarios. La validación queda pendiente.
