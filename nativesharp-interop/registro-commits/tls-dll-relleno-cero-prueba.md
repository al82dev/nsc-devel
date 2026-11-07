# Reproducción de TLS de DLL con relleno cero

El fixture usa el productor y consumidores de `validar-escalar-crt.cmd`, que
pasaron C y NS O0–O3. Copia los ejecutables y la DLL a una carpeta temporal y
convierte los últimos cinco bytes cero de la plantilla TLS en relleno cero
declarado por el directorio PE. Comprueba que esos bytes originales sean cero
antes de modificar la copia. La plantilla pasa de 69 a 64 bytes y el relleno
de 0 a 5; el estado inicial de las variables y el tamaño total se conservan.

En el compilador previo, el control C devolvió 0 y el consumidor NS O0 falló
con código 135. `llvm-readobj --coff-tls-directory` confirmó la plantilla y
los cinco bytes de relleno. El helper que genera NSC contrasta la dirección
exportada solo con `EndAddressOfRawData`, excluyendo `SizeOfZeroFill`. El
fixture original permanece sin cambios. La prueba aún no acredita corrección.
