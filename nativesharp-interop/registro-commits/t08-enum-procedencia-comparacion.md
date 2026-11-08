# Comparación de procedencia de enums

Se prepara una comparación que exige contrato y USR coincidentes, ruta al
mismo archivo físico, hash del buffer y desplazamiento iguales, y que el
archivo actual aún corresponda a ese hash. Se usa para enums independientes
y para dependencias enum de un record. La ausencia de datos no prueba
equivalencia. Quedan pendientes construcción y pruebas del conjunto.
