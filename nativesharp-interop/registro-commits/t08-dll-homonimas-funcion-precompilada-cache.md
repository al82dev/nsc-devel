# Función C precompilada con DLL TLS homónimas

La identidad de la DLL de origen se conserva junto a la referencia de una función C capturada en el catálogo vigente. Al enlazar un consumidor, el driver recupera esa identidad de los módulos precompilados y la contrasta con las DLL TLS seleccionadas antes de generar la importación al alias. Una identidad ausente o ambigua no se asigna arbitrariamente. No se agregaron versiones de NSMeta.

La prueba construye una biblioteca NativeSharp cuya plantilla pública lee una variable TLS y llama una función C de su DLL; el consumidor utiliza también la TLS de otra DLL homónima. Sin encabezados C en el consumidor, se comprobó una compilación inicial y una restauración real desde caché tras borrar el ejecutable y ambas DLL empaquetadas; el ejecutable restaurado se ejecutó correctamente. El conjunto pasó en O0–O3 junto a la prueba previa de funciones C con encabezado.

La caché para proyectos que importan directamente encabezados C/C++ sigue rechazada por NSC hasta rastrear encabezados transitivos. La prueba usa una ruta de caché corta: con una ruta más larga se observó que el bundle no se publicaba; su causa y soporte general de rutas largas quedan pendientes. El traslado se verificó entre carpetas del mismo equipo, no en otra computadora. No se cierran los casos ambiguos, transitivos ni la tanda 8.
