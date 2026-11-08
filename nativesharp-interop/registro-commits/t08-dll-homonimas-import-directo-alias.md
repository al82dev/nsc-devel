# Importación directa de funciones C y TLS con alias

Cuando hay DLL TLS homónimas, el driver identifica funciones importadas que Clang atribuye a una única DLL, obtiene el alias según el contenido de esa DLL y agrega la biblioteca de importación del alias antes de las bibliotecas de entrada. Las declaraciones con orígenes ambiguos o que no se pueden proyectar no se asignan arbitrariamente. La validación previa de importaciones de sistema ocurre antes de añadir los alias locales.

El consumidor NativeSharp de prueba usa dos DLL homónimas, llama directamente a `tls_read` y a la función que crea un hilo, y comprueba que la función C y la variable TLS de NativeSharp comparten estado en el hilo principal y el secundario. La tabla de importaciones contiene el alias y no `api.dll`; el ejecutable y los alias se copian a otra carpeta y se ejecutan desde `A:\`. La prueba dirigida pasó en O0–O3 y la regresión de rutas Unicode pasó. El caso existente sin importación directa también restauró caché en los cuatro niveles.

La nueva prueba de importación directa usa `--no-artifact-cache`: no demuestra restauración de ese consumidor desde caché ni ejecución en otra computadora. Tampoco acredita funciones publicadas solo por catálogos precompilados, símbolos con origen ambiguo, importaciones transitivas o el producto DLL NativeSharp. Esas variantes y la tanda 8 siguen abiertas.
