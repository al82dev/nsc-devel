# Tanda 8: dependencia transitiva de DLL TLS dinámica

El fixture construye una DLL `api2.dll` con una variable TLS e importación real de `leaf.dll`. El segundo productor declara ambas dependencias; el consumidor emplea dos plantillas que resuelven variables TLS homónimas de `api.dll` y `api2.dll`.

`validar.cmd` comprobó en el PE que `api2.dll` importa `leaf.dll`, publicó el consumidor en una carpeta aislada, verificó que las tres DLL están presentes y ejecutó el programa con su control de dos hilos. La prueba pasó dos veces consecutivas con O0 y se incorporó al validador del bloque. No se ha ejecutado la batería completa de la tanda 8.
