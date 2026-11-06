# Referencia C de TLS estático con LLD

El objeto C que utiliza `__declspec(thread)` y comprueba independencia entre
dos hilos se enlazó con `lld-link` 23.1.0 y las bibliotecas estáticas del
toolset disponible en A:. El ejecutable de referencia usa un punto de entrada
Win32 propio y fuerza `_tls_used`; se ejecutó con resultado cero y emitió
`TLS_LLD_PROBE_PASS`. Entradas y salidas se encuentran en
`A:/nsc-interop-build/tanda08-tls-c/probe-lld.c` y `probe-lld.cmd`.

Esta observación confirma la viabilidad de TLS para ese caso C con LLD. No
demuestra todavía la integración con NSC, la inicialización de un ejecutable
NativeSharp, el TLS exportado por una DLL ni la cobertura de otras bibliotecas.
NSLink y LLVM no se modificaron.
