# Acceso generado a TLS de DLL

El fixture directo mostró que una declaración C TLS sin `dllimport` en una
referencia con runtime se proyectaba como TLS del ejecutable. El instrumento
separado demostró que la dirección importada identifica los datos iniciales
de la DLL y permite obtener el slot del hilo mediante el directorio TLS PE.

El inventario distingue referencias estáticas y con runtime; rechaza orígenes
mixtos, DLL diferentes y triples distintos de Windows x64 MSVC. El descenso
declara el símbolo como dato importado, genera un helper local de resolución
por hilo y conserva el tratamiento anterior para bibliotecas estáticas. Se
añade `GetModuleHandleExA` al conjunto de importaciones Win32 disponible para
que el enlazador existente pueda resolverlo. Se comprueban firmas PE, presencia
del directorio y de slots antes de devolver la dirección.

**Pendiente de validación:** construcción incremental y pruebas positivas y
negativas agrupadas. La implementación usa detalles internos de Windows x64;
no demuestra compatibilidad de otras plataformas, todos los tipos TLS ni la
validación integral de tanda 8. No cambia NSLink ni LLVM externo.
