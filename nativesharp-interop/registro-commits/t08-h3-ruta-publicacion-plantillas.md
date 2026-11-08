# T08-002 H3: ruta actual de plantillas con dependencias C

Se revisaron en `e0c24c25c` el proveedor del catálogo C, la publicación del
texto de plantillas y su materialización precompilada. La revisión distingue la
observación de demandas de enum del inventario necesario para cuerpos genéricos
y registra dónde el catálogo actual retorna vacío.

El diagnóstico y las preguntas pendientes están en
`T08-002-PROPUESTA-METADATA-C.md`. Se actualizó `TRACKING.md` sin cerrar H3.
No se cambió código del compilador ni el formato NSMETA. La evidencia funcional
previa se conserva en el fixture `pruebas-tanda-08-identidad-c/plantillas`; no
se ejecutó nuevamente en esta revisión estática.

Una lectura posterior del recolector muestra que las llamadas sin argumentos de
tipo explícitos no entran en `collectCallDependency`; se agregó ese dato y el
requisito de comprobar caché fría y caliente a la propuesta.

También se verificó por lectura que el driver solo emite funciones y globales
C internas si el modelo del productor ya las seleccionó. Publicar sus firmas
sin seleccionar los objetos deja incompleta la solución de H3.
