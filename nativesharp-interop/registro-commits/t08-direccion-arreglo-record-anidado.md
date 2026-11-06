# T08-010: dirección de arreglo en record C anidado

SCOPE LOCK: corregir en NSC la dirección de un elemento de arreglo C anidado en
un record empaquetado; mantener acceso directo y no modificar LLVM ni NSLink.
El fixture `PackedNestedArrayAddress.ns.txt` compiló con código 0 y produjo IR,
aunque el record exterior garantiza alineación 1 y el elemento int requiere 4.

El análisis de índices examinaba solo la alineación nominal del campo arreglo,
perdiendo la restricción propagada por el record receptor. Para campos C/C++
importados, el elemento conserva ahora la restricción de una base mutable no
direccionable. Punteros indizados y arreglos no importados no cambian. Falta
reconstruir incrementalmente y ejecutar las pruebas dirigidas y de la tanda.
