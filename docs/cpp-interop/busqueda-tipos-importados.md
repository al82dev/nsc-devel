# Búsqueda de tipos y namespaces importados

El modelo de búsqueda recibe namespaces y declaraciones C++ antes de construir
los índices. Valida identificadores y dimensiones físicas y asocia cada tipo
importado con su ordinal para consultas posteriores.

Los tamaños y alineaciones transportados no definen por sí solos los campos ni
las reglas de vida útil del tipo. El encabezado conserva un include duplicado
que se corregirá por separado después de registrar el estado acumulado.
