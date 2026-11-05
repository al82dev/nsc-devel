# Tanda 08: entrada a resolución semántica de typedef

La resolución de tipos consulta la expansión C/C++ después de los aliases
NativeSharp y antes de enlazar los tipos hijos. Un fallo detiene el enlace del
tipo afectado. El método de resolución se implementa como siguiente cambio de
la misma tanda; este commit es un paso intermedio y no constituye una versión
validada ni habilita el proveedor en el driver.
