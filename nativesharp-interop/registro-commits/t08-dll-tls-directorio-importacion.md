# Control del directorio de importación en la DLL TLS

En el control C existente, una DLL sin directorio de importación observa un
valor TLS incorrecto. Se copió el binario a una carpeta de pruebas y se agregó
un directorio de importación que contiene exclusivamente el descriptor nulo;
no se agregó ninguna dependencia ni se modificó el binario original.

En esta máquina, el ejecutable de control observa índice TLS 0 y valor
`0x80000000` antes del cambio, y el control dinámico termina con 106. Con el
descriptor vacío observa índice 1 y valor inicial 7, y el control dinámico
termina con 0. La prueba ejecutable es
`pruebas-tanda-08-identidad-c/tls-dll-distintas/inspeccionar-directorio-importacion.py`.

Esto identifica una diferencia en la carga de estos binarios concretos; no
demuestra la causa interna de Windows ni autoriza alterar DLL de terceros.
Queda pendiente decidir e implementar una solución que respete la identidad de
las bibliotecas, su integridad y el empaquetado. La tanda 8 sigue abierta.
