# Identidad de enumeradores C

La proyección conserva el USR de cada enumerador y la selección acumula la
identidad conocida antes de producir el símbolo. Dos USR conocidos distintos
se rechazan aunque los valores coincidan; la ausencia no borra la identidad
conocida. No se altera persistencia y falta la validación agrupada.
