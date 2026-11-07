# Prueba: productor precompilado y TLS de DLL C

El productor importa un global TLS de una DLL C y expone lecturas, escrituras
y una comprobación de aislamiento. El consumidor solo referencia la biblioteca
NativeSharp y no importa el header C. Se prepara la matriz O0–O3; queda sujeta
al resultado real y no sustituye los controles C que fallan.
