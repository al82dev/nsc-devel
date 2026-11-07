# T08: contraste de directorio de importación vacío en DLL TLS

## Evidencia

Se enlazó `A:/nsc-interop-build/t08-tls-contraste-import/noimport.dll` con
`lld-link` oficial, `/dll /noentry /nodefaultlib /include:_tls_used`, el
`api.obj` utilizado en el contraste anterior y `libcmt.lib`. `llvm-readobj`
mostró un directorio TLS y ausencia de directorio de importación. La DLL no
tiene punto de entrada. El objeto corresponde al fixture `ScalarApi.c`.

El instrumento `pruebas-tanda-08-tls-c/directo/aislar_directorio_importacion.py`
produce dos copias de esa imagen, con el índice TLS inicializado al centinela
`0xffffffff` para observar si el cargador lo cambia. En la segunda copia, y
solo en esa, el directorio de importación apunta a un descriptor nulo de veinte
bytes ubicado en relleno de `.rdata`; no se agrega una dependencia DLL ni se
modifica código ejecutable. Cada copia se carga en cinco procesos Python
separados y se observa el índice y la llamada C `tls_zero_read`.

En Windows `10.0.28000.3231`, los cinco procesos de la imagen sin directorio
conservaron el centinela y la lectura C produjo una excepción de acceso. Los
cinco procesos con directorio vacío observaron índice `0x7` y lectura cero.
El valor del índice depende del proceso. El instrumento es de observación y no
presenta estas salidas como una prueba integral automática.

Reproducción: enlazar el objeto guardado con el comando indicado arriba y ejecutar
`python nativesharp-interop\pruebas-tanda-08-tls-c\directo\aislar_directorio_importacion.py`.
Las copias y el objeto se conservan en `A:/nsc-interop-build/t08-tls-contraste-import`.

## Interpretación y límites

La presencia de un directorio de importación vacío fue suficiente para cambiar
el resultado en este experimento: no fue necesario importar una función Win32.
Esto no identifica el mecanismo interno del cargador ni demuestra una solución
general. Alterar una imagen PE en el experimento no es una modificación propuesta
al enlazador ni al compilador. Los controles originales permanecen intactos y
fallando; NSLink no se modificó y la tanda 8 sigue abierta.
