# T08-004: dirección del índice TLS de DLL

El validador exige que llvm-readobj muestre el índice TLS y que su dirección
no sea cero, además de ejecutar el consumidor. Falta validación posterior de
este cambio del control.
