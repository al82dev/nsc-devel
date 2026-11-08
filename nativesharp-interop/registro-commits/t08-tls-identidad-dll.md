# Tanda 8: identidad de DLL para variables TLS C

El compilador conserva la identidad de la DLL de origen de una variable TLS C importada al publicar y restaurar declaraciones usadas por plantillas. La identidad combina el nombre de la DLL y la huella de su contenido. Las declaraciones TLS de DLL solo se concilian cuando sus identidades conocidas coinciden.

Esto evita que dos DLL distintas con una variable del mismo nombre se unifiquen silenciosamente. La resolución separada y el enlace de ambas DLL siguen pendientes: el caso ahora produce un diagnóstico de ambigüedad y no constituye interoperabilidad completa.

Validación dirigida: compilación incremental de `nsc` en A; una DLL compartida por uno y dos productores pasó en O0–O3, incluso entre hilos. La reproducción de dos DLL distintas rechazó el programa por ambigüedad en vez de generar un ejecutable que apuntaba únicamente a la primera DLL. No se ejecutó la batería completa de la tanda 8.
