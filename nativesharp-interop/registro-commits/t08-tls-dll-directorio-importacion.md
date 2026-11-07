# Tanda 8: contraste del directorio de importación de la DLL TLS

## Reproducción y resultado

El instrumento `pruebas-tanda-08-tls-c/directo/probe_tls_import_directory.py` tomó las DLL sin importaciones generadas por `validar-escalar-carga-dinamica.cmd` y `validar-nsc-escalares.cmd`. En copias temporales en `A:/nsc-interop-build/tls-abi` puso `0xffffffff` en el índice TLS. En dos variantes cambió únicamente la entrada PE del directorio de importación para que apuntara a un descriptor de veinte bytes vacío, tanto en padding como dentro del tamaño virtual de `.rdata`; no agregó una dependencia DLL ni cambió el código de las funciones.

En la máquina de la prueba, cargadas mediante `ctypes.WinDLL` en el mismo proceso, la copia sin directorio conservó el índice `0xffffffff`; las copias con descriptor vacío recibieron índices, por ejemplo `0x7` y `0x8`. En ambas la función C `tls_zero_read` devolvió cero tanto en el hilo principal como en un hilo nuevo. El instrumento `probe_tls_import_nsc.cmd` ejecutó el mismo consumidor NativeSharp O0 con la DLL original y con una copia experimental que conserva el índice inicial original y todas las exportaciones: devolvió 8 con la original y 0 con la copia. Esa copia difiere de la original únicamente en la entrada del directorio PE.

Una primera comparación NativeSharp usó una copia que además tenía el centinela inicial; se repitió con la copia que conserva el índice original para aislar la entrada del directorio. Para reproducir, ejecutar `validar-nsc-escalares.cmd` —que termina con el fallo original y deja su DLL y ejecutable—, después `python probe_tls_import_directory.py` y finalmente `probe_tls_import_nsc.cmd`, usando las rutas del directorio `pruebas-tanda-08-tls-c/directo`.

## Alcance y límites

Este contraste identifica una diferencia reproducible en esas imágenes y en el entorno probado. No demuestra el mecanismo interno del cargador ni establece una regla para todas las versiones de Windows. Leer cero en dos hilos comprueba disponibilidad e inicialización, no aislamiento de escrituras; el consumidor NativeSharp sí ejercita lecturas y escrituras escalares en el hilo principal. El instrumento localiza zonas de veinte bytes cero dentro de `.rdata` y en su padding, y rechaza el fixture si no existen. La zona dentro de la sección puede compartir bytes con estructuras ya presentes, que permanecen inalterados.

No se alteró la DLL original, ningún fixture ni NSLink. La diferencia no es una corrección de NSC y no autoriza parchear bibliotecas ajenas. El control C que no enlaza, las variantes de DLL originales y la validación completa siguen fallando; la tanda 8 permanece abierta.
