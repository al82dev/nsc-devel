# T08: global C utilizada por una plantilla pública precompilada

Una plantilla pública NativeSharp puede referirse a una global C del productor. Al consumir la biblioteca sin su encabezado, el compilador necesita conservar esa declaración en el catálogo de dependencias C y restaurarla para resolver la plantilla.

`provideCppCRecordMetadataCatalog` incorpora las globales candidatas capturadas al catálogo. `provideCppValuesWithPrecompiledCatalogs` las proyecta para las consultas visibles y comprueba su identidad frente a declaraciones ya presentes. La verificación de metadata acepta únicamente globales externas con identidad y símbolo coherentes; las globales TLS permanecen fuera de esta ruta.

Validación dirigida: la biblioteca estática de `pruebas-tanda-08-global-template` se compila a O0; un consumidor separado, sin encabezado, instancia la plantilla, modifica la global y obtiene el valor mediante una función C. El inspector confirma la presencia de la global en el NSMETA. La prueba terminó con código 0. No acredita otras optimizaciones ni cierra la tanda 8.
