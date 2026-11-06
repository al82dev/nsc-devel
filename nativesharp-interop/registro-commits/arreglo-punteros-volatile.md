# Proyección de arreglos C de punteros a volatile

La importación rechazaba explícitamente el arreglo aunque ya existe soporte
para punteros simples a objetos volatile. Se reutiliza el tipo de cada elemento
para proyectar el calificador del objeto apuntado y se propaga al indexar,
igual que el contrato readonly existente. Se rechaza tomar la dirección del
slot porque esa operación requeriría preservar calificadores anidados.

El cambio está preparado; faltan los fixtures y su validación agrupada. No se
afirma interoperabilidad funcional ni cierre de T08-015 o T08-016.

La revisión de `CppAliasBuiltinType.h` mostró que el helper general rechaza
`volatile`; por ello el camino de arreglos proyecta este caso mediante el
mapeador del elemento, que conserva las restricciones de profundidad del
puntero. Los demás tipos no representables siguen rechazándose.
