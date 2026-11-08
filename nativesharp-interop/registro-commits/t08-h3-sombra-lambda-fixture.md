# T08 H3: parámetro de lambda homónimo de una global C

El productor añade una plantilla que crea una lambda sin capturas mediante un
typedef de callback C. El parámetro de la lambda tiene el nombre de una global
C; el consumidor precompilado debe obtener 23 y el control de metadata debe
confirmar que la global no se publica por esa referencia local.

El resultado y cualquier limitación del compilador se registrarán por separado.
