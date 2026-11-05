# Producción de metadatos con referencias C++

La producción añade el catálogo de dependencias binarias C++ y eleva la versión
del formato cuando se utiliza. Las declaraciones importadas no se publican como
si fueran definiciones NativeSharp propias. La poda de la superficie precompilada
conserva el catálogo de dependencias C++.

La expresión de comprobación de vida útil también se reescribe sin cambiar su
resultado. Las firmas públicas que mencionan tipos importados requieren revisión
adicional; este commit no certifica que ya estén representadas completamente.
