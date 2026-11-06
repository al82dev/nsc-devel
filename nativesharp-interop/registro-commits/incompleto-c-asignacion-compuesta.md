# Asignación compuesta de puntero a C incompleto

Con el compilador anterior, `Opaque* value = opaque_get(); value += 1;`
falló durante el descenso porque no se puede determinar el tamaño del
elemento. Se preparó la comprobación en `finishPointerCompoundAssignment` para
reportar el tipo C incompleto durante el análisis. El caso y la corrección
quedan pendientes de la validación agrupada; los demás tipos conservan su ruta.
