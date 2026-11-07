# T08: importación de punteros atómicos globales adicionales

El control `diagnosticar-punteros.cmd` compiló C y NSC rechazó los tres globales
atómicos a float, long y unsigned long C. El importador reconocía una lista fija
de apuntados que no los incluía. Se preparan exclusivamente esos tres apuntados
no TLS y no calificados de 32 bits y sus claves NativeSharp; se conservan
restricciones TLS, de tamaño y alineación.

El soporte de `float*` necesita además conservar la operación atómica en HIR y
LLVM. Esta etapa no se ha construido ni validado; no demuestra interoperación.
No se modifican NSLink, LLVM externo o metadata.
