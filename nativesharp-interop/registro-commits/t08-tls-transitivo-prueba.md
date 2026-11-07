# Prueba de TLS transitivo precompilado

El productor NativeSharp solo llama una función C: el TLS y los archivos del
CRT se obtienen de sus dependencias publicadas, sin referencia TLS directa en
el objeto NativeSharp. El consumidor comprueba emisión directa O0–O3 y
`nsc build` con selección automática de LLD y ejecución. Las salidas se
escriben en A:. El caso no cubre las DLL C cuyos controles siguen abiertos.
