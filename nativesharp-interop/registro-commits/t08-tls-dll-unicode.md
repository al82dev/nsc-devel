# Tanda 8: nombre y salida Unicode para DLL TLS C

La prueba construye `api-漢.dll`, fuera de la página de códigos activa, como dependencia de un productor NativeSharp. Genera un ejecutable y una DLL NativeSharp en un directorio que también contiene `漢`; el ejecutable consume dos variables TLS de DLL diferentes desde una carpeta de trabajo distinta. Después retira el ejecutable y sus DLL, confirma un hit de compilación completa y comprueba los archivos restaurados y su ejecución.

Para este recorrido, NSC interpreta las rutas de referencias como UTF-8, conserva nombres Unicode en la identidad TLS y sus claves de caché, usa UTF-8 al reconstruir la ruta Windows y prepara un nombre temporal ASCII para bibliotecas que NSLink no puede abrir por su nombre original. El contenido de la biblioteca no se altera y no se modifica NSLink ni se introduce versionamiento de metadata.

`tls-dll-unicode/validar.cmd --cache` pasó en O0, incluida la emisión de la DLL NativeSharp y la restauración desde caché. También pasaron los controles dirigidos de DLL preexistente y de concurrencia en O0–O3, y la regresión de empaquetado de una DLL C en una DLL NativeSharp. El fixture no certifica todos los caracteres Unicode, nombres de fuentes, equivalencias de mayúsculas/minúsculas ni el consumo de la DLL NativeSharp generada. La batería completa de la tanda 8 sigue pendiente.
