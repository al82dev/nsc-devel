# Prueba de campos de bits C volatile

El fixture compara structs normales y empaquetados con campos volatile con y sin signo. C inicializa y comprueba; NativeSharp lee y modifica. Se conserva IR O0 generado por Clang para estudiar ancho, alineación y orden. La creación del fixture no demuestra compatibilidad.

El primer intento rechazó los campos por no estar proyectados. En la muestra, el layout IR de Clang identifica unidades de 32 bits en el offset 0 para el struct normal y offset 1 para el empaquetado. Las operaciones son `load volatile i32` y `store volatile i32`, con alineación 4 y 1 respectivamente, sin orden atómico en el IR observado. Se agregaron funciones equivalentes y volcado IR de NSC para comparar después de la corrección.

Tras proyectar la unidad de almacenamiento, `validar-volatile.cmd` pasó en O0–O3. El control automático confirmó en cuatro funciones equivalentes que Clang y NSC usan la misma cantidad de accesos volatile i32, alineaciones 4 o 1 y ningún orden atómico. También pasaron los fixtures de struct, union y metadata no volatile en O0–O3 y `nsc.cpp-interop.session`. Esta evidencia no cubre todas las combinaciones de tipos y layouts, unions volatile ni metadata precompilada de volatile.
