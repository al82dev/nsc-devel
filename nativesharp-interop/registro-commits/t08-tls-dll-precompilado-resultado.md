# Resultado: TLS de DLL C con productor precompilado

`validar-dll-precompilado.cmd` pasó O0–O3. El consumidor sin referencia al
header invocó al productor, leyó y escribió el TLS de la DLL C y contrastó el
valor con una función C, incluyendo aislamiento de otro hilo. La DLL se
construyó con importaciones reales. Salidas en
`A:/nsc-interop-build/tls-abi/dll-precompiled-20087-22843`.
Se incorpora el caso al gate, sin eliminar los cuatro controles C que fallan;
el resultado no cierra la tanda.
