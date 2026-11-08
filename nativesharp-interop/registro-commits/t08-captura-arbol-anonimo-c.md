# Captura del árbol de agregados anónimos C11

La sesión de Clang captura los agregados anónimos de un record C mediante una pila,
con ruta de índices de campos desde el propietario, clase struct o union, tamaño,
alineación, campos y offsets. La prueba incorpora una union con struct anónimo
anidado y comprueba la ruta y su layout.

La información queda en la instantánea de la sesión; aún no se proyecta ni se
serializa. El cambio no habilita el consumo de miembros anónimos. La construcción y
las pruebas agrupadas siguen pendientes, y T08-010/T08-011 permanecen abiertos.
