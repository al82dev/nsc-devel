# Tanda 8: selección para bitcode sin TLS

Una biblioteca C compilada con `-flto=thin` sin variables TLS reproducía NSL1003: el selector dejaba NSLink, que no consume el objeto bitcode de máquina `0x4342`. El mismo inspector que determina TLS para `nsc build` identifica ahora bitcode como entrada que requiere LLD. La inspección de COFF TLS, la selección explícita y la selección de NSLink para COFF sin TLS se preservan. NSLink y las fuentes de LLVM no se modifican.

Las pruebas dirigidas ThinLTO y LTO completo pasaron con y sin TLS; los controles de COFF sin TLS y las selecciones explícitas también pasaron. Queda pendiente la validación completa y continúan abiertos los fallos de DLL.
