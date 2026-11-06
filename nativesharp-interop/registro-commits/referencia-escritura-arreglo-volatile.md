# Referencia de escritura de arreglos volatile

Se agrega al fixture una función C que escribe el arreglo global y la matriz
volatile. Permitirá comparar el orden emitido por Clang con las asignaciones
equivalentes de NativeSharp. La función se prepara como referencia; no se
afirma todavía ningún resultado de su compilación ni se altera producción.
