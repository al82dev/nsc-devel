# Tipo del índice de GEP en helper TLS

La construcción incremental rechazó pasar la dirección de un `const auto`
a `LLVMBuildGEP2`, cuya API recibe un arreglo mutable de referencias LLVM.
Se elimina el `const` local del índice; no cambia el valor de IR generado.
Pendiente repetir la construcción y las pruebas agrupadas.
