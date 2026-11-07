# Control de exportación TLS externa

La matriz LLD inspecciona las exportaciones de la DLL con `llvm-readobj` y
rechaza que aparezca el accessor inexistente `__nsc_get_shared_tls`. El
proyecto conserva sus métodos NativeSharp públicos y debe enlazar la DLL.
Este control está preparado y se ejecutará con la construcción corregida.
