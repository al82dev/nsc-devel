# T08: TLS atómico precompilado a través de DLL

Se amplió el control precompilado con una DLL NativeSharp que enlaza la biblioteca productora estática y expone métodos a un ejecutable independiente. El encabezado C está oculto durante la compilación de la DLL y su consumidor. Se comprueba el directorio TLS PE de la DLL y el comportamiento de los tres tipos atómicos del fixture.

El primer enlace falló por `_tls_index`: el proyecto productor no declaraba el CRT que lo proporciona. Se añadieron las referencias explícitas al CRT estático, siguiendo la configuración del control transitivo existente; no se modificó NSLink ni el código de producción. La cadena estática y la cadena con DLL pasaron O0–O3 con el compilador de A: y LLD.

La prueba comprueba los accesos en el hilo principal, no el aislamiento entre hilos ni todos los contratos atómicos. Los cuatro controles DLL originales y la validación integral de T08 siguen pendientes.
