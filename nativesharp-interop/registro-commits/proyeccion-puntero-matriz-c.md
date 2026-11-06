# Proyección de puntero C a arreglo fijo escalar

La función `c_matrix_ptr(const CMatrix*)` no se importaba porque
`mapFunctionType` carecía de proyección del arreglo apuntado. Se prepara el
mapeo de dimensiones fijas para escalares C y se conserva el calificador del
apuntado en la función. Se mantienen rechazados volatile y elementos anidados
que aún no tienen representación comprobada. Falta validación de extremo a
extremo y controles negativos de calificadores y dimensiones.

Con `915a25d711461ecf` el proyecto real compiló, enlazó y ejecutó en O0–O3:
C leyó la matriz NativeSharp y también la modificó mediante un parámetro
mutable. Se rechazaron pasar el retorno de matriz const y otra matriz con
dimensiones distintas a la función mutable. La suite de sesión pasó 1/1.
Las pruebas cubren matrices escalares fijas; no cubren elementos record,
enum, puntero o volatile ni todos los caminos de escape.
