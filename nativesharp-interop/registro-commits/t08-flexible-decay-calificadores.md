# T08: propagar const y volatile al puntero flexible

El miembro flexible importado registra por separado si su almacenamiento admite
conversión, independientemente de la restricción de tomar su dirección sin
calificador. La calificación del elemento se propaga al apuntado. La ruta
excluye origen subalineado, temporal no direccionable y calificadores anidados
de punteros que no se pueden preservar con esta representación.

El análisis de conversiones y HIR consumen ese mismo hecho; los controles
existentes de pérdida de calificadores siguen siendo responsables del destino.
Pendiente de construir y validar con casos positivos y negativos.
