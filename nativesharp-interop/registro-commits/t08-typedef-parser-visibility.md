# Tanda 08: nombres de typedef en el parser

El inventario léxico incorpora los nombres cualificados de aliases para los
proyectos visibles y los transforma de la notación de Clang `::` a la escritura
del parser. Los ámbitos anónimos se excluyen de este inventario. El cambio se
aplica tanto al parseo ordinario como a la copia para observación. Reconocer un
nombre no lo resuelve: falta instalar su tipo y manejar colisiones en semántica.
El proveedor todavía no se activa desde el driver y la validación está pendiente.
