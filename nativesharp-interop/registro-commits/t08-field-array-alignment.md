# T08-010 / T08-012 / T08-016: alineación de elementos de campo arreglo

Una dirección de elemento calculada desde un campo de arreglo no transmitía
la alineación del campo a las cargas y escrituras. Un helper privado recorre
los nodos MIR de indexación hasta un FieldAddress y obtiene la alineación
garantizada como máximo común divisor de esa alineación y los strides. La
carga y las escrituras simples y compuestas la indican a LLVM. Si el origen no
es un campo o la información falta, conserva la ruta anterior.

Esto no habilita arreglos empaquetados: falta proteger la dirección de sus
elementos y verificar el circuito completo. Pruebas y validación agrupadas
pendientes.
