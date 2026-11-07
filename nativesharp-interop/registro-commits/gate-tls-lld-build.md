# Inclusión de TLS con LLD en la batería de la tanda 08

El gate agrupado incorpora los controles de selección, emisión de ejecutable,
`nsc build` y cambio de header transitivo con LLD. Se conservan los ensayos
originales de TLS estático con NSLink y TLS directo de DLL: añadir estos casos
no convierte sus fallos en éxito ni reduce la cobertura exigida. La batería
completa deberá repetirse tras resolver los pendientes de TLS.
