# Resultado: TLS C a través de una DLL NativeSharp

`validar-dll-precompilado.cmd` terminó con código 0 para O0–O3. Además del
consumidor directo, produjo y ejecutó una cadena de DLL C, biblioteca estática
NativeSharp, DLL NativeSharp y ejecutable NativeSharp; se comprobaron lectura,
escritura y aislamiento del otro hilo. La copia del header C estuvo ausente
durante la compilación de los dos últimos productos. Los archivos están en
`A:/nsc-interop-build/tls-abi/dll-precompiled-21103-23262`.
Los cuatro controles C anteriores y la validación completa siguen pendientes.
