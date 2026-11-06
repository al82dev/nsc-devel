# Selección explícita de LLD para el enlace

NSC permite que `NSC_LINKER` identifique explícitamente `lld-link` además de
`nslink`, resolviendo la ruta con el mismo procedimiento y sin cambiar la
selección predeterminada. Los diagnósticos identifican el producto correcto;
se siguen rechazando ejecutables ajenos. Los comandos explícitos de emisión
todavía deben validarse con la selección nueva.

Faltan construcción incremental, prueba, configuración del CRT y enlace TLS
de extremo a extremo. La ruta de DLL y la tanda completa permanecen abiertas.
