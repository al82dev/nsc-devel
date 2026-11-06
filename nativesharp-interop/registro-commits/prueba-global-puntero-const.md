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

Se agregan dos controles negativos: asignar un elemento a un `int*` mutable y
tomar la dirección del slot como `int**`. Ambos deben rechazar la pérdida del
calificador; todavía no se ejecutaron con la proyección habilitada.

Validación posterior con build `207aab239dce1116`: lectura y reasignación del
slot, con verificación desde C, pasaron O0–O3. Los controles negativos
rechazaron la escritura al dato, la conversión a `int*` y la dirección del slot;
el último ahora emite un diagnóstico específico. El fixture de layout C pasó
O0–O3. No se acreditan aún matrices, otros tipos ni todas las rutas de escape.

Se preparan dos controles adicionales de pérdida de const: un puntero local
inferido desde el elemento y el paso del arreglo a una función C que espera
`int**`. Los diagnósticos deben inspeccionarse; aún no se han ejecutado.
