# Construcción LLVM de objetos C++

La bajada usa almacenamiento del tamaño de registro C++ observado, permite
construir en el destino local y busca el destructor al descartar un temporal.
Las instrucciones de declaración y expresión pasan el destino o solicitan la
limpieza según corresponda.

Se trata de una ruta experimental sujeta a validación conjunta de ABI, vida
útil y excepciones; este registro no demuestra todos los casos de C++.
