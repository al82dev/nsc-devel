# TLS transitivo de biblioteca precompilada

Una biblioteca NativeSharp con `Bridge.CheckWorker()` que llama a una función C
se compiló como archivo precompilado. Su objeto solo declara la función C como
indefinida; el TLS está en `api.lib`, registrado junto con el CRT en la metadata.
El consumidor directo sin selección explícita eligió NSLink y falló con NSL1023.
Los artefactos reproducidos están en `A:/nsc-interop-build/tls-abi/transitive-probe`.
Se delimita la corrección anticipada de la selección, sin modificar NSLink.
