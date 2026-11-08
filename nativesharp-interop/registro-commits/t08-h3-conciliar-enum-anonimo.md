# T08 H3: conciliación de enumerador anónimo con encabezado

Cuando el consumidor incluía el mismo encabezado que la biblioteca, la constante viva y la restaurada desde NSMETA quedaban ambiguas. Para conciliar este caso, el productor conserva en la referencia una huella SHA-256 del archivo de origen cuando está disponible. El consumidor compara esa huella, USR, nombre, tipo y valor; solo concilia si existe un candidato coincidente único.

El validador de identidad pasó en O0 para el mismo encabezado y también conservó la ambigüedad esperada para un encabezado independiente con el mismo nombre de constante y valor. Las metadata anteriores sin huella siguen siendo legibles, pero no se concilian mediante esta evidencia. La prueba no demuestra todos los casos de preprocesamiento ni cierra la tanda 8.
