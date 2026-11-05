# Tanda 08: restrict en niveles de punteros de typedef

Se conserva restrict como metadato del modificador de puntero al expandir
typedefs escalares, enum y record, tambien en arreglos y componentes de
callbacks compuestos. El dato no es una promesa LLVM noalias. Faltan las rutas
de callbacks simples y los casos de uso y validacion agrupada.
