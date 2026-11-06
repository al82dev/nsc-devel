# Global C const struct

La proyección admite un struct C global const con identidad y layout verificados,
conservando const en el estado semántico. No habilita arreglos ni punteros a
records calificados; la lectura y la protección de campos requieren validación.
