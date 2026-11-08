# T08: precisión sobre la salida de validadores

Se revisó la conclusión de falso éxito. Al lanzar directamente los scripts
anteriores con la herramienta se observó salida cero en fallos inducidos. Al
invocar esas mismas versiones mediante un script que utiliza CALL y comprueba
ERRORLEVEL, como hace el gate, los tres casos siguientes devolvieron error:

- Productor inválido en global-static: salida uno y WRAPPER_FAILED.
- Ejecutable de global atómica que devolvió 42: salida uno y WRAPPER_FAILED.
- DLL inválida dentro de la subrutina TLS precompilada: salida uno y
  WRAPPER_FAILED.

Las pruebas anteriores no demuestran que el gate principal aceptara fallos.
Los cambios posteriores hacen explícitas las salidas y permanecen verificados
en las rutas indicadas por sus registros; no se certifica el gate completo ni
se extrapola este resultado a todos los scripts. Para pruebas directas hay
que comprobar sus marcadores finales o capturar el estado desde CALL.
