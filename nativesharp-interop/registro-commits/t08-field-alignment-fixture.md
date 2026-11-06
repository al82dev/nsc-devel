# Prueba de alineación de accesos a campos C

El fixture de layout prepara una lectura y escrituras simple y compuesta del
campo `Pair.b`, con comprobación cruzada del valor mediante una función C. Un
verificador examina las instrucciones LLVM y exige alineación 4 para cada
forma de acceso. La ejecución está pendiente de la validación agrupada de la
tanda y no acredita todavía el comportamiento de campos empaquetados.
