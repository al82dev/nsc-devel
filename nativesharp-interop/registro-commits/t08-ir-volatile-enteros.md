# Comprobación del IR para enteros volatile

El validador del fixture exige cargas acquire y escrituras release para cada
global de 8, 16 y 64 bits, con y sin signo. La ejecución mixta comprueba los
valores; esta comprobación añade el contrato observable en IR. La batería
agrupada está pendiente.
