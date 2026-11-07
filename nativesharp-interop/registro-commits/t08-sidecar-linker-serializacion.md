# Tanda 8: preservar selección implícita al guardar proyectos

`nsc new` guardaba `linker: NSLink` sin elección del usuario; un proyecto nuevo que consumía la biblioteca TLS falló en NSLink. `nsc add package` también insertaba esa clave al reescribir un sidecar que la omitía. El serializador conserva ahora la ausencia de la clave, y sigue escribiendo una selección que sí estaba presente. El valor predeterminado interno continúa siendo NSLink cuando no se necesita LLD.

No se migran sidecars ya existentes: un `linker: NSLink` almacenado se respeta, porque no es posible distinguir una selección deliberada de una generada por versiones anteriores. La construcción incremental y las pruebas dirigidas de proyecto nuevo, TLS, bitcode y selecciones explícitas pasaron en A:. La validación completa de la tanda sigue pendiente.
