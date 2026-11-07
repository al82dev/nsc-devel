# T08-002: propuesta de metadata para funciones, globales y alias C

Se añade `T08-002-PROPUESTA-METADATA-C.md` y el fixture
`pruebas-tanda-08-identidad-c/plantillas`, que el validador ejecuta: el
consumidor de métodos no genéricos que usan C compila y ejecuta; los consumidores
que instancian plantillas públicas con funciones, globales o typedefs C solo se
observan y hoy fallan, también con el encabezado propio. La propuesta define la
sección mínima del catálogo C, su selección, la restauración, la conciliación por
USR y las pruebas. No modifica NSMETA.
