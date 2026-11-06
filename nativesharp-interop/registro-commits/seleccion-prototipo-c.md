# Selección de prototipo en redeclaraciones C

La sesión de Clang conserva un único registro por entidad. Si existen redeclaraciones C compatibles, selecciona la declaración más reciente que conserva un prototipo, o busca otro prototipo en la cadena si hace falta. Las funciones C realmente sin prototipo y las declaraciones C++ mantienen su tratamiento. La prueba de aridad y las regresiones se ejecutan juntas.
