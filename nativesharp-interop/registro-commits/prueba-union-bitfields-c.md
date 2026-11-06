# Prueba dirigida de campos de bits en union C

La prueba superpone campos de bits con un entero y un arreglo de bytes. C inicializa y comprueba el valor; NativeSharp lee y escribe campos con y sin signo. El script contempla O0–O3. La preparación del fixture no demuestra compatibilidad.

El primer intento falló en O0 porque `first` y `delta` no se proyectaban. Tras el cambio en `CppSemanticRecord.cpp`, la construcción incremental y `validar-union.cmd` pasaron O0, O1, O2 y O3. También pasaron en el mismo grupo `validar.cmd` y `validar-metadata.cmd` de structs C en O0–O3 y `nsc.cpp-interop.session`. No se probaron campos volatile ni la publicación precompilada de esta union; el ítem y la tanda siguen abiertos.
