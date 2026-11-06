# Puntero a función C retornado y llamado desde NativeSharp

El fixture prepara el retorno de un callback C, una llamada indirecta desde
NativeSharp y su paso a C para devolverlo y volver a invocarlo. El script
compiló, enlazó y ejecutó estos casos de O0 a O3 con código 0. La prueba usa
una firma escalar `int(int)` y no cubre todas las firmas o convenciones.
