# Tanda 08: arreglos globales C const

- La proyección recoge el calificador `const` de niveles de arreglo o elemento y lo conserva como inmutabilidad del estado externo; mantiene la identidad y dimensiones fijas.
- Se restringe a elementos escalares o enum sin punteros; se mantiene el rechazo de `volatile` y límites incompletos. El indexador existente debe propagar la inmutabilidad a los elementos.
- Pendiente de validación agrupada con lecturas y rechazos de escritura. Este cambio no cierra T08-015 ni cubre todas las combinaciones de calificadores.
