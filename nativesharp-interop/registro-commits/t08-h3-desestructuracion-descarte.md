# T08 H3: patrones anidados y descarte

El identificador `_` de un patrón de desestructuración es un descarte, no una
referencia a la global C homónima. El filtrado de demandas excluye todos los
identificadores dentro del patrón; conserva el análisis del inicializador.

El control `t08-h3-desestructuracion-descarte-control.md` reproduce el fallo
en O0–O3. La validación tras la corrección queda pendiente de construcción
incremental.
