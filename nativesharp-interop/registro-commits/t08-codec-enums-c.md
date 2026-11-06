# Codec conjunto de records y enums C

Se agrega una envoltura que transporta el catálogo de records y enums, con tipo
subyacente, valores exactos e identidades. La lectura vuelve a codificar para
comprobar representación canónica y conserva las salidas ante error. El codec
de solo records permanece disponible como sección interna y no lee enums.
Faltan integración completa y ejecución agrupada.
