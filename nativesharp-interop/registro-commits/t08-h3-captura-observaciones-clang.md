# T08 H3: captura con observaciones reales de Clang

Se amplio la prueba de dos encabezados para pasar a las capturas preparadas una funcion y una global seleccionadas por los selectores existentes, asi como una observacion de typedef proporcionada por una sesion real de Clang. Se verifican simbolos y USR; para el alias este control no demuestra que una plantilla lo haya seleccionado semanticamente.

Compilacion incremental y ejecucion de `nsc_cpp_c_identity_tests` con Clang 23.1 en A:: codigo 0. La seleccion dentro de cuerpos genericos, sombras, definiciones internas, publicacion, restauracion y el gate de tanda siguen pendientes. No se modifica el compilador en este commit.
