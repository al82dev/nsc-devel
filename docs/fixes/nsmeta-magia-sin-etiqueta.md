# Magia NSMETA sin etiqueta de formato (NSC-META-001)

El escritor usa la magia `NSMETA\0\0`. Los archivos anteriores a la retirada de las etiquetas llevan
`NSMETA1\0`. `readMetadataHeader` comparaba los ocho bytes exactos, así que un archivo así fallaba
con "invalid NativeSharp metadata magic" antes de leer nada de su contenido: un error por número de
versión, contra el contrato de NSC-META-001.

El lector acepta ahora el prefijo `NSMETA`, un byte de etiqueta cero o dígito que no selecciona
nada, y el octavo byte cero. Lo que sigue (marcador de orden de bytes, hash de la carga y huella
ABI) decide si el contenido se puede leer.

`tests/integration/nsmeta_magic_label.py` (`nsc.compiler.nsmeta-magic-label`) produce una biblioteca
con el compilador bajo prueba. Su metadata carga tal como se escribió y con el dígito restaurado en
la magia; un prefijo dañado y una etiqueta no numérica se siguen rechazando como magia inválida.
También pasan las pruebas `metadata`/`nsmeta` existentes con sus ejecutables reconstruidos.

Sigue abierto `nsc.compiler.operator-in-legacy-escape`: su fixture binario (build `14717501b74375ce`)
pasa ahora la magia, pero falla con "truncated or invalid metadata type record", porque la forma de
sus registros de tipo no es la que el lector actual decodifica. Es un límite de contenido, no de
etiqueta, y se registra aparte.
