# Control del mapeo de long double

Se amplía la prueba escalar existente para distinguir un `long double` de 64
bits sin formato acreditado, que debe rechazarse, de uno con evidencia IEEE
binary64, que debe mapear a double. Aún no se ha construido ni ejecutado.
