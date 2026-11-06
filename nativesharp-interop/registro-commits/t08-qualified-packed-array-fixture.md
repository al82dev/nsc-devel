# Prueba preparada de arreglos C empaquetados calificados

El fixture prepara lectura de un arreglo const y lectura/escritura de otro
volatile dentro de un record `pack(1)`. El IR debe conservar volatile y
alineación 1; dos casos negativos intentan escribir el arreglo const y exponer
la dirección de un elemento volatile. La ejecución sigue pendiente y el caso
no cubre todas las combinaciones de calificadores o elementos.
