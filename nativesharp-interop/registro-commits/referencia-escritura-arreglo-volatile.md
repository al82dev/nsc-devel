# Referencia de escritura de arreglos volatile

Se agrega al fixture una función C que escribe el arreglo global y la matriz
volatile. Permitirá comparar el orden emitido por Clang con las asignaciones
equivalentes de NativeSharp. La función se prepara como referencia; no se
afirma todavía ningún resultado de su compilación ni se altera producción.

Clang 23 compiló `Api.c` con `--target=x86_64-pc-windows-msvc -std=c11
-O0 -S -emit-llvm`. En el IR resultante, ambos accesos de escritura de
`write_volatile_arrays` son `store atomic volatile i32` con orden `release`
y alineación 4. El IR NS previo usa `store volatile i32` sin ese orden para
el arreglo y la matriz. La comparación está limitada a estos accesos y al IR
NS anterior, que debe regenerarse al validar los cambios pendientes.
