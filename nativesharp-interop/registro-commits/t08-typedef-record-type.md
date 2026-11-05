# Tanda 08: referencia a record desde un typedef

`cppAliasRecordType` obtiene de la identidad capturada por Clang el nombre fuente
de un record y conserva sus niveles de puntero y `const` representables. Excluye
ámbitos anónimos y especializaciones de esta proyección inicial y no inventa
layout, vida o ABI. El nombre debe contrastarse con el índice del proyecto antes
de enlazarlo; la prueba agrupada sigue pendiente.
