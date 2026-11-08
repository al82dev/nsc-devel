# Estados independientes de globales C internas

Dos bibliotecas independientes incluyen encabezados homónimos con globales
`static int` del mismo nombre, inicialmente 11 y 29. Cada biblioteca publica
plantillas de lectura y modificación de su propia global.

El consumidor comprueba ambos valores iniciales, incrementa el primero a 14 y
verifica que el segundo siga en 29; luego incrementa el segundo a 34 y verifica
que el primero siga en 14. La prueba dirigida `static-dos-bibliotecas-mutables`
compiló los productores y el consumidor y ejecutó el caso correctamente en O0.
El validador adicional `validar-optimizaciones.cmd` reconstruyó los productores
y el consumidor para cada optimización y ejecutó el mismo caso en O0–O3. Esta
muestra no cierra la identidad para todas las categorías.
