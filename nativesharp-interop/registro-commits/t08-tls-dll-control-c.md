# Tanda 8: control C para las dos DLL con TLS

La reproducción de dos DLL con `tls_exported` ahora comprueba primero, desde un ejecutable C independiente, que ambas bibliotecas devuelven sus respectivos valores iniciales: 3 y 7. La segunda DLL incorpora un acceso a `GetCurrentThreadId` y una función de lectura propia.

Durante el diagnóstico, la segunda DLL construida sin importaciones devolvió un valor incorrecto incluso desde su propia función C, tanto al cargarla al iniciar el proceso como mediante `LoadLibraryA`. Al agregarle una importación de Kernel32, el control observado devolvió 7. Este resultado describe la configuración probada; no atribuye una causa general al cargador de Windows.

Resultado de `validar.cmd dos`: control C correcto; NSC rechaza después las referencias de las plantillas como ambiguas. La selección experimental por proyecto permitió compilar pero el enlace volvió a asociar ambas variables con una sola DLL, por lo que se retiró. La resolución por DLL y el cierre de este caso siguen pendientes. Una biblioteca de importación experimental generada con un alias mediante `llvm-dlltool` solicitó el nombre del alias como exportación y no resolvió el problema.
