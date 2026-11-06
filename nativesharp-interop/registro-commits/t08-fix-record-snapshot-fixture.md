# Tanda 08: ubicación del fixture de capturas repetidas

La suite agrupada de sesión falló en cuatro comprobaciones que dependían de la
misma captura. El selector exige que las definiciones repetidas indiquen un
archivo físico equivalente; el fixture utilizaba una ruta virtual. Se creó un
archivo de prueba real y se conservó la verificación de identidad del selector.
Se añade el motivo de fallo para facilitar el siguiente diagnóstico.

La corrección queda pendiente de reconstrucción y ejecución agrupadas.
