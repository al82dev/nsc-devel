# Tanda 8: control de DLL TLS con el mismo nombre

La reproducción publica los productores NativeSharp en `uno` y `dos`, cada uno con su propia biblioteca y su propio `api.dll`. Un control C carga ambos archivos por sus rutas relativas, comprueba que son módulos diferentes y lee los valores TLS 3 y 7. Después, el consumidor NativeSharp reproduce y comprueba el diagnóstico de seguridad para DLL distintas con el mismo nombre.

`validar-mismo-nombre.cmd` terminó con código 0: el control C pasó y NSC produjo el diagnóstico esperado. Ese resultado demuestra la reproducción y la detección, no la interoperabilidad. Sigue pendiente conservar las rutas separadas hasta el ejecutable y hacer que NSC resuelva cada DLL por su ruta; también debe verificarse su cierre de dependencias y empaquetado. La reproducción anterior, que ponía ambos productores en un solo directorio, no demostraba el caso de productores independientes.
