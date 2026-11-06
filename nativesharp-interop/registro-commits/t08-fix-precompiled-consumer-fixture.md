# Tanda 08: módulos distintos en el fixture de consumidor

La prueba de sesión detectó que el consumidor sintético reutilizaba el nombre
del módulo productor y el grafo lo rechazaba antes de probar interoperabilidad.
El fixture ahora crea un proyecto consumidor independiente y conserva el catálogo
en el módulo productor. La corrección está pendiente de ejecución agrupada.
