# Validación de arreglos en agregados anónimos C

Se ejecutó `pruebas-tanda-08-anonimos-c/validar.cmd` tras el commit
`b867e4285`. Terminó con código 0: los accesos cruzados de C y NativeSharp
a arreglos naturales y empaquetados pasaron de O0 a O3 en los caminos directo,
sesiones repetidas, biblioteca con encabezado y biblioteca con metadata.
El comparador existente confirmó además los accesos volátiles que ya incluía el
fixture. Esta ejecución no compara el IR de alineación de los arreglos y no
constituye la validación completa de la tanda 8.
