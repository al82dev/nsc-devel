# T08-010/015: referencia para acceso volatile por alias local

`VarVolatileMode.c` y `VarVolatileMode.ns` leen y escriben mediante un alias
local de un miembro flexible `volatile int[]`. Los dos proyectos NativeSharp
seleccionan respectivamente Microsoft e ISO para la sesión de Clang. Las
fuentes aíslan los accesos para contrastar orden y alineación del IR.
La ejecución de la comparación queda pendiente; esto no corrige el compilador.
