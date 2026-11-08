# T08 H3: codec interno para tipos de dependencias C

Se prepara un codec interno de `CppType` para las futuras instantaneas de dependencias de plantillas. Reutiliza el recorrido existente de tipos y conserva calificadores, tipos anidados y la evidencia de formato de `long double`; acepta tambien `Atomic`. Los codecs anteriores siguen rechazando `Atomic` y conservan su representacion: esta pieza no altera NSMETA ni se conecta todavia a la publicacion.

Se agrego un control de ida y vuelta con puntero, atomico y formato flotante, mas el rechazo de datos truncados sin alterar la salida. La construccion y ejecucion quedan pendientes para agruparlas con los demas cambios de la tanda. Aun faltan la captura de dependencias, sus identidades y simbolos, la integracion del formato, la restauracion y la validacion completa.
