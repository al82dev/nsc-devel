# Tanda 8: selección automática para `--link-lib`

La selección de enlace directo solo inspeccionaba referencias del proyecto. Una biblioteca con TLS aportada mediante `--link-lib` provocó NSL1023 en el ejecutable y generó una DLL sin directorio TLS. El mismo proyecto DLL con LLD explícito sí generó el directorio. El selector inspecciona ahora las bibliotecas adicionales antes de calcular la identidad de caché y de enlazar, tanto en ejecutables como en DLL, conservando elecciones explícitas.

La construcción incremental y los controles dirigidos pasaron en la unidad A: para ejecutable y DLL O0–O3, consumidor NativeSharp de la DLL en O0, bitcode sin TLS, COFF sin TLS y selecciones explícitas. También pasaron las regresiones de selección directa por referencias de proyecto. No se modifica NSLink y no se declara resuelta la batería ni los cuatro controles de DLL C originales.
