# T08: control de global C bool exportado por DLL

El fixture prepara una DLL C con un `volatile _Bool` exportado, un control C
independiente y un consumidor NativeSharp. Se contemplan lectura, escritura,
modificación compuesta y comprobación de accesos atómicos volátiles en LLVM IR
en la configuración MS. Los ejecutables NativeSharp se preparan para O0–O3.

El control está preparado sin ejecutar; no demuestra compatibilidad ni cierra
T08-004 o T08-015. No modifica NSLink.
