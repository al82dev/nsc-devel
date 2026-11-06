# Arreglo global C de punteros a datos const

El fixture prepara un arreglo C `const int *[2]`, compartido entre C y
NativeSharp. La prueba positiva lee ambos datos, cambia un elemento por otro
puntero a const y pide a C comprobar el cambio. Una prueba negativa intenta
escribir a través del puntero. La salida se conserva en A: y la matriz cubre
O0–O3. Aún no se ha ejecutado y no demuestra soporte.

Con build `66725a00ab433111`, Clang compiló la biblioteca y NSC rechazó el
caso positivo en O0: `cannot import C/C++ global 'c_slots': C/C++ global type
is not yet mapped`. No se llegó al enlace ni al control negativo. El inventario
rechaza expresamente arreglos con `pointeeConst`; antes de habilitarlos hay que
trazar cómo se preserva ese calificador al indexar y desreferenciar.
