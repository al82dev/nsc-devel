# Reproducción: raíz de biblioteca y enlazador

La cadena sin TLS con metadata intermedia aislada compiló correctamente, pero
el JSON de `nsc build` informó `linker: LLD`; se esperaba NSLink porque la
clausura se resuelve en `LibraryReferenceRoot` y no contiene TLS. Registro:
`A:/nsc-interop-build/tls-abi/raiz-reproduccion.log`.
