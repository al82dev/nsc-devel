# Contratos de conversión a enum C

Se amplía la prueba con un entero variable, conversión a enum C sin signo de
32 bits y control negativo para la inicialización de un enum NativeSharp.
Pendiente ejecución O0–O3 y validación del rechazo concreto.
Se agregan también un origen de 64 bits que requiere truncamiento y uno de
8 bits con signo que requiere extensión a 32 bits.
El caso previo de arreglo enum se vuelve a inicializar desde NativeSharp con
enumeradores C para comprobar la limitación original en su contexto real.
