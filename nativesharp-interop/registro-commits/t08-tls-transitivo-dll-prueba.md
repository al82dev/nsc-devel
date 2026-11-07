# TLS transitivo en DLL y selección explícita

Se amplía la prueba transitiva para producir una DLL NativeSharp que enlaza
la biblioteca precompilada y su dependencia TLS. Para O0–O3 se comprueban el
directorio TLS y la ejecución de un consumidor NativeSharp. También se comprueba
que `--linker NSLink` siga respetándose al construir el consumidor transitivo.
Estos casos no sustituyen los controles C de DLL que siguen fallando.
