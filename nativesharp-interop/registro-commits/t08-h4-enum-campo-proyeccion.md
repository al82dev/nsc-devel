# T08-002 H4 parte B: campo C de enum anónimo

Para un campo C escalar de enum anónimo sin typedef, se proyecta el entero que
Clang señala como subyacente cuando tamaño y alineación coinciden. Se conservan
los calificadores y el tipo C original; no se cambia metadata ni se inventa una
identidad de enum NativeSharp. La validación de la combinación con la captura
está pendiente.
