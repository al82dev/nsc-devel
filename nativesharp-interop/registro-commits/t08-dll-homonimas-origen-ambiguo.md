# Dos DLL con el mismo símbolo de función

El control construye dos DLL llamadas `api.dll`, cada una con una variable TLS y una función exportada `tls_read`. El consumidor NativeSharp referencia ambas bibliotecas y solicita esa función. NSC emite el diagnóstico de importación directa que todavía no puede distinguir y no publica el ejecutable. El control pasó en O0 junto con las pruebas existentes de DLL homónimas.

Esto comprueba que este caso no se resuelve mediante una elección arbitraria; no implementa la desambiguación ni acredita otros escenarios. La interoperabilidad para ambos orígenes permanece pendiente en T08-002-09m7c1d2b.
