# Alcance de globales C volatile void*

El importador de funciones C ya reconoce `volatile void*`, pero el inventario
de globales lo excluye de la lista de pointees volatile. Se acota la extensión
al puntero inmediato y se conservan los rechazos para punteros anidados.
Los controles se agruparán con los de la tanda 08.
