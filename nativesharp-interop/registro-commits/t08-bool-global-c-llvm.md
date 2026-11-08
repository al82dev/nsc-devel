# T08-006: almacenamiento de globales bool C

Las declaraciones LLVM de globales C bool se representan como bytes,
conservando las dimensiones de arreglos. Las cargas convierten el byte a valor
bool y las escrituras amplían el valor a byte; el camino volatile existente
conserva sus reglas de orden. Pendiente construcción y validación.
