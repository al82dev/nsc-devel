# T08: función C static inline en plantilla precompilada

La reproducción del commit `6462227ed` terminaba con `unknown identifier` para la función `fixture_twice` al instanciar una plantilla publicada. La captura excluía la declaración interna; el verificador rechazaba su forma de enlace y la generación de objetos solo atendía funciones seleccionadas en el modelo ordinario.

La captura admite definiciones C `static inline` entre las candidatas de la plantilla. El verificador acepta esa identidad únicamente con enlace interno y sin enlace de lenguaje. El generador incluye el objeto Clang cuando una demanda de plantilla visible menciona la función. La firma y el símbolo seleccionados viajan en el catálogo C existente.

Tras la construcción incremental, la prueba de productor y consumidor separados pasó en O0, O1, O2 y O3; el consumidor no recibe el encabezado. La regresión agrupada de alias, funciones externas y globales pasó en los cuatro niveles. No se acredita con esta prueba toda la semántica de cabeceras ni se cierra la tanda 8.
