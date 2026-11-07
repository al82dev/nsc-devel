# T08: proyección de long C atómico global en Windows x64

La matriz ampliada compiló las fuentes C y NSC rechazó `_Atomic(long)` y
`_Atomic(unsigned long)` como tipos no mapeados. La proyección exigía los
nombres `int` y `unsigned int`, aunque el mapeador escalar usa signo y ancho y
la muestra de Windows usa 32 bits para ambos tipos C long.

Se aceptan esos dos nombres únicamente para globales no TLS con los mismos
requisitos de tamaño y alineación de la proyección existente. La ruta TLS y los
demás tipos permanecen sin ampliar. Faltan construcción y validación de IR,
operaciones y regresión; no se modifica NSLink.
