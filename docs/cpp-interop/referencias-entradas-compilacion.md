# Referencias C++ en el manifiesto de entradas

El manifiesto incorpora las rutas de encabezados y bibliotecas de referencias
C++. El código actual rechaza calcular la identidad de caché si encuentra estas
referencias mientras sus dependencias transitivas no estén integradas.

La restricción puede impedir compilaciones con referencias C++: queda como
comportamiento actual por revisar, no como interoperabilidad completa.
