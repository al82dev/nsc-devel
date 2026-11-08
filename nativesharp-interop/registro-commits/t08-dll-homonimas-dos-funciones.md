# Funciones homónimas de dos DLL TLS

Una función C restaurada del catálogo conserva ahora su identidad de DLL en el modelo semántico. Cuando dos módulos precompilados aportan el mismo símbolo desde DLL TLS homónimas, cada función recibe un nombre interno distinto y resuelve el símbolo en el alias de su propia DLL. La búsqueda usa el mismo mecanismo que la TLS para localizar la DLL junto al módulo y mantenerla cargada. Los nombres internos codifican el símbolo y la identidad, y el camino se activa únicamente si existen las TLS homónimas correspondientes.

La prueba usa dos plantillas NativeSharp precompiladas que llaman a funciones C llamadas `tls_read`, cada una en su propia DLL. Cada función ve el valor TLS escrito por su plantilla. Se comprobó la construcción y ejecución en O0–O3 y la restauración del ejecutable y ambas DLL desde caché en los cuatro niveles. También pasaron los controles existentes, incluido el rechazo de una llamada directamente ambigua en un mismo ámbito.

La prueba de las dos funciones usa un solo hilo y una sola computadora. La colisión de funciones variádicas se diagnostica como no implementada; otros modelos de DLL, resolución por encabezados y dependencias transitivas permanecen pendientes. No se modificó NSLink ni se introdujeron versiones nuevas de NSMeta. La tanda 8 sigue abierta.
