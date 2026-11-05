# Tanda 08: enums C en firmas de funciones

La proyección de funciones con índice de tipos puede conservar la identidad de
un enum C importado en parámetros, resultados y punteros. Comprueba el tipo
subyacente, tamaño, alineación y ausencia de conflicto antes de proyectarlo;
no transforma el enum en un entero anónimo. Los casos dependientes de tipos no
seleccionados, la ejecución y el ABI requieren validación agrupada.
