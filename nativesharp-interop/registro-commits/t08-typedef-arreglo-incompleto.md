# Tanda 08: expansion de typedef de arreglo incompleto

Se prepara la expansion de typedefs C T[] y T[][N] con identidad de limite
desconocido, para elementos escalares, enums o records reconocidos. Se conserva
el puntero escrito en el uso y se diagnostica sizeof/alignof del arreglo sin
limite. No se usa el marcador de array flexible. Faltan pruebas agrupadas y
casos de calificadores no representables.
