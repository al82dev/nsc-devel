# Prueba de bit-fields en NSMETA sin selección por versión (NSC-META-001)

`nsc.item328.msvc-bitfield-layout` exigía que un modelo con `formatMinor = 17` fuera rechazado al
escribirse, con el mensaje "before 1.18 cannot represent bit-field layout". Esa comprobación
seleccionaba conducta por el número de versión, que NSC-META-001 retiró: la escritura ya no depende
de la etiqueta y conserva el contenido (`df19082da`). Con el ejecutable de prueba reconstruido
(antes estaba obsoleto, ver `binario-de-prueba-vigente.md`), la prueba fallaba en esa sola
comprobación de 20.

La comprobación pasa a medir el contenido: el mismo modelo con la etiqueta 1.17 se escribe, se lee de
vuelta y debe conservar las coordenadas exactas del bit-field con nombre (`B`, desplazamiento 0, bit
1, ancho 3) y la barrera sin nombre de ancho 0. Si la escritura volviera a rechazar por versión, o
perdiera los bit-fields, la prueba falla. Las demás comprobaciones no cambian.

Resultado: 20 de 20 comprobaciones.
