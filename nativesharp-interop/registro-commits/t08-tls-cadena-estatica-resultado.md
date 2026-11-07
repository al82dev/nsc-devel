# Resultado: TLS detrás de dos bibliotecas precompiladas

La construcción incremental de `nsc` terminó correctamente. La prueba agrupada
`validar-tls-transitivo.cmd` terminó con código 0: la cadena con dos bibliotecas
NativeSharp y TLS C pasó en emisión directa y `nsc build` con LLD; una cadena
equivalente sin TLS conservó NSLink. Pasaron también los controles previos de
selección, DLL y caché incluidos en el script. Registro:
`A:/nsc-interop-build/tls-abi/cadena-estatica-validacion.log`.
La resolución temprana conservadora de rutas alternativas no fue examinada por
esta prueba. Los cuatro controles de DLL C y la tanda siguen abiertos.
