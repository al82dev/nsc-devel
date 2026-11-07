# Control negativo de arreglo TLS volatile sin signo

El control negativo mantiene un tipo aún no proyectado:
`volatile unsigned int[3]`. Los arreglos escalares sin ese calificador se
ejercitan en el fixture positivo y esperan la validación agrupada.
