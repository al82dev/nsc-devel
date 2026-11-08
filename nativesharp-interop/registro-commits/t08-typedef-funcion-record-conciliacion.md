# Typedefs de función con records precompilados

Una plantilla pública que solo usaba un typedef de función no incorporaba el
record incluido en su firma al catálogo C. Ahora se captura su definición y su
cierre; al restaurar dos productores se comparan las firmas y los records con
la comparación estructural existente. Para ese caso precompilado se permite
comparar los contratos completos guardados cuando ambos encabezados faltan;
un encabezado existente pero modificado no se considera evidencia válida.
El lookup mantiene el alias del productor al analizar su propia plantilla.

La muestra de dos typedefs de función con `CRecord*` equivalente compiló y se
ejecutó en O0–O3 con los encabezados ocultos. Se rechazaron en O0 dos records
incompatibles, incluido `int` frente a `float` del mismo tamaño, y un encabezado
modificado después de publicar la biblioteca. La regresión dirigida del typedef
variádico escalar pasó en O0. Se añadió un validador al cierre de la tanda.

La muestra declara el puntero a función: todavía no prueba invocación real con
el record, records anidados, enums, caché ni todas las variantes de identidad.
La tanda 8 sigue abierta. No se introdujo una versión nueva de NSMETA.
