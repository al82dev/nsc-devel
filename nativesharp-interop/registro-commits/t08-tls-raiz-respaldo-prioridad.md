# Control de prioridad de la raíz explícita

El fixture conserva un `NS_LIB_ROOT` distinto y no existente mientras establece
`--library-reference-root` válido. La compilación debe usar la raíz explícita,
conservar NSLink para la cadena sin TLS y ejecutar el resultado.
