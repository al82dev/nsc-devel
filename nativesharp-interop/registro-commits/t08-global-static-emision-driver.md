# T08-004: objeto Clang para global C estatica utilizada

El driver agrupa las globales internas seleccionadas con las funciones inline
de su misma sesion y pide un solo objeto. No solicita definiciones no usadas.
La construccion incremental y la validacion estan pendientes.
