# Prueba negativa de proyección TLS

La prueba negativa de TLS estático usa ahora un puntero todavía no admitido.
El `volatile int` anterior deja de ser negativo al incorporarse a la prueba
de escalares. La comprobación sigue exigiendo el diagnóstico de capacidad no
proyectada y se ejecutará con el resto de la validación agrupada.
