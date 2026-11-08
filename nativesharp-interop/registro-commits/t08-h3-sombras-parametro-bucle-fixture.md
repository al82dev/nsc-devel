# T08 H3: parámetros y bucles que sombrean una global C

El productor añade plantillas públicas con un parámetro y un inicializador de
`for` llamados igual que una global C. El consumidor comprueba los valores
19 y 3, respectivamente. El control existente también inspecciona que la global
C no se publique cuando todas sus referencias pertenecen a esos ámbitos locales.

Este cambio prepara cobertura adicional; la ejecución y su resultado se
registrarán por separado. No acredita todavía todo el alcance de nombres.
