# Resultado: selección con raíz de biblioteca

La construcción incremental de NSC y `validar-tls-transitivo.cmd` terminaron
con código 0. Con dependencias en `LibraryReferenceRoot`, la cadena TLS usó
LLD; la cadena sin TLS conservó NSLink en `nsc build` y coincidió con NSLink
explícito en emisión directa. Ambos ejecutables se ejecutaron. Se incluyen los
controles previos del script. Registro:
`A:/nsc-interop-build/tls-abi/raiz-validacion.log`.
La raíz alternativa de respaldo no se probó por separado. Los cuatro controles
TLS de DLL y la validación completa de la tanda siguen pendientes.
