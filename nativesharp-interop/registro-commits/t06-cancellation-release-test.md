# Tanda 06: liberación tras cancelar

El sistema de archivos de prueba señala su destrucción. Tras cancelar una
consulta real de header se liberan el AST y la referencia de prueba, y se exige
que el VFS quede destruido. Esto comprueba ese recurso concreto, no todas las
asignaciones internas de Clang. Pendiente de validación agrupada.
