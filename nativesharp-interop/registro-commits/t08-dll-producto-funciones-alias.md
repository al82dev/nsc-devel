# Funciones C de DLL homónimas en una DLL NativeSharp

Al agregar llamadas a funciones C junto con los accesos TLS de dos DLL
homónimas, el producto NativeSharp fallaba con un diagnóstico de importación
ambigua. El enlace de ejecutables ya incorporaba importaciones con alias antes
de enlazar; el enlace de DLL no llamaba a esa misma preparación.

La preparación de la DLL ahora aplica la selección compartida de alias, que
solo acepta funciones con procedencia no ambigua. La prueba autosuficiente
`tls-dll-producto/validar.py` escribe la TLS y la lee mediante una función C
de cada DLL, comprueba el aislamiento en dos hilos y restaura el paquete desde
caché. `llvm-readobj` confirma dos nombres de alias y las dos funciones, sin
importar `api.dll` por su nombre original. La prueba pasó en O0–O3 y se
compiló NSC incrementalmente.

Este caso usa funciones con nombres distintos y bibliotecas precompiladas;
las importaciones transitivas ambiguas continúan pendientes.
