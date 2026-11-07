# Resultado: NS_LIB_ROOT y bibliotecas transitivas

La construcción incremental de NSC y la prueba agrupada
`validar-tls-transitivo.cmd` terminaron con código 0. `nsc build` resolvió la
clausura mediante `NS_LIB_ROOT`, eligió LLD con TLS y NSLink sin TLS; la emisión
directa también pasó. Una raíz explícita conservó prioridad frente a un valor
distinto de `NS_LIB_ROOT`. Registro:
`A:/nsc-interop-build/tls-abi/respaldo-validacion.log`.
No se repitió la validación integral; los cuatro controles TLS de DLL siguen
abiertos y no se modificaron.
