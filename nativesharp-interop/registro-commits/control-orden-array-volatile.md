# Controles de orden para arreglos globales C volatile int

El fixture verifica ahora que las lecturas de arreglo y matriz lleven
`atomic volatile` con `acquire` y que sus escrituras lleven `release`, ambos
con alineación 4. Una asignación compuesta añade el caso de lectura y escritura
de un mismo elemento. Estos controles y la ejecución aún están pendientes de
la batería agrupada.

La prueba del proveedor de globales comprueba por separado el origen C y el
origen C++ para evitar activar la nueva ruta en declaraciones C++.

Se prepararon controles directos del cálculo de alineación con dos dimensiones,
alineación ausente, modo no MS, stride subalineado y raíz local. Las pruebas
siguen sin ejecutarse; no se consideran evidencia de éxito todavía.
