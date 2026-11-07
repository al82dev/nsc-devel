# Tanda 8: contraste del directorio de importación de la DLL TLS

## Reproducción y resultado

El instrumento `pruebas-tanda-08-tls-c/directo/probe_tls_import_directory.py` tomó la DLL sin importaciones generada por `validar-escalar-carga-dinamica.cmd`. En dos copias temporales en `A:/nsc-interop-build/tls-abi` puso `0xffffffff` en el índice TLS. En la segunda cambió únicamente la entrada PE del directorio de importación para que apuntara a un descriptor de veinte bytes vacío en padding ya existente de `.rdata`; no agregó una dependencia DLL ni cambió el código de las funciones.

En la máquina de la prueba, cargadas mediante `ctypes.WinDLL` en el mismo proceso, la copia sin directorio conservó el índice `0xffffffff`; la copia con descriptor vacío recibió el índice `0x7`. En esta segunda variante la función C `tls_zero_read` devolvió cero tanto en el hilo principal como en un hilo nuevo. La prueba se ejecutó dos veces con el mismo resultado del índice; la lectura en dos hilos se comprobó en la segunda ejecución.

## Alcance y límites

Este contraste identifica una diferencia reproducible en esas imágenes y en el entorno probado. No demuestra el mecanismo interno del cargador ni establece una regla para todas las versiones de Windows. Leer cero en dos hilos comprueba disponibilidad e inicialización, no aislamiento de escrituras. El instrumento presupone el padding RVA `0x2300` del fixture y lo valida antes de modificar las copias.

No se alteró la DLL original, ningún fixture ni NSLink. La diferencia no es una corrección de NSC y no autoriza parchear bibliotecas ajenas. El control C que no enlaza, las variantes de DLL originales y la validación completa siguen fallando; la tanda 8 permanece abierta.
