# Inicializador de identidad sintética

La compilación de `92d212d3e` advirtió que el fixture de un tipo record
sintético no inicializaba explícitamente `clangUsr`. Se establece `nullopt`,
conservando su comportamiento previo. La desaparición de la advertencia se
verificará en la siguiente construcción agrupada.
