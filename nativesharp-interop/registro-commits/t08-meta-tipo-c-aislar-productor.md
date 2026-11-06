# Aislamiento de publicación de tipo C

Se añade un productor que devuelve null en vez de llamar a la función C, para
separar la resolución del nombre de la publicación de la firma y campo C.
El script no construye el compilador ni ejecuta la batería completa. Esta
variante no verifica el consumidor ni la lectura efectiva del dato.
