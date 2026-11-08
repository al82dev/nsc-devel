# Diagnósticos e identidad de rutas Unicode

El cargador de proyectos conserva las rutas UTF-8 en los errores de referencias
de proyecto, biblioteca y DLL. Un proyecto con `Name` explícito ya no intenta
convertir innecesariamente su nombre de archivo a la página de códigos local.

Validación: construcción incremental de NSC y `tls-dll-unicode\validar.cmd
--cache --unicode-source`, con resultado 0. Se comprobó con `os.path.samefile`
que Windows considera la biblioteca `ref-É.lib` y `ref-é.lib` el mismo
archivo y que NSC la rechaza como referencia duplicada conservando el nombre
Unicode en el diagnóstico. `ref-Ê.lib` es otro archivo y ambas referencias se
aceptan. También se comprobó `Proyecto-漢.nsproj` con `Name` explícito.

Este control prueba esas rutas concretas en el sistema de archivos usado; no
demuestra todas las equivalencias Unicode ni cierra la tanda 8.
