# Reproducción: clausura con NS_LIB_ROOT

`nsc build` con `NS_LIB_ROOT` encontró la biblioteca intermedia local, pero la
preparación de clausura no buscó su dependencia `PlainProducer` en esa raíz y
falló antes de enlazar. Registro:
`A:/nsc-interop-build/tls-abi/respaldo-reproduccion.log`.
