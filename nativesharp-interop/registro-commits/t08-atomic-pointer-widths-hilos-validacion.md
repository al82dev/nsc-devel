# T08: aislamiento entre hilos de punteros atomicos TLS

`validar-widths-hilos.cmd` termino correctamente en O0, O1, O2 y O3. El hilo C
observo los valores iniciales TLS, escribio sus cuatro punteros y termino; el
hilo NativeSharp retuvo las direcciones y valores propios. El control se agrega
al gate de la tanda 8, que no se ejecuto en esta validacion dirigida.
No demuestra solapamiento de operaciones ni apuntados calificados. T08-004 y
T08-015 permanecen abiertos.
