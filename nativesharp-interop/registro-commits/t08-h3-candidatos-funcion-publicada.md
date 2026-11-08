# T08 H3: candidatos de función para plantillas publicadas

El selector relaciona las demandas conservadas por compilación con las claves
presentes en el catálogo de plantillas publicadas. Reutiliza la selección de
funciones por proyecto, namespace, nombre y visibilidad, y devuelve los índices
de inventario originales y los conflictos diferidos. Las alternativas son
candidatos, no decisiones sobre sombras, sobrecargas ni dependencias C finales.

El control usa sesiones Clang para dos encabezados con una función externa
común; al publicar una plantilla conserva ambos orígenes y excluye la otra
plantilla. La construcción incremental del objetivo y la ejecución dirigida
terminaron con código 0. Se observaron tres advertencias preexistentes de
inicializadores en `CppSemanticRecord.cpp`.

El selector todavía no alimenta la escritura ni la lectura de NSMETA. Faltan
globales, aliases, cierre de tipos, resolución semántica y verificación por un
consumidor precompilado. No se ejecutó el gate completo de la tanda 8.
