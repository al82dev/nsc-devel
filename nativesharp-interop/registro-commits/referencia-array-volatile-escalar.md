# Referencia C de arreglos volatile escalares

El fixture declara arreglos de `_Bool`, enteros de 8, 16 y 64 bits con ambos
signos, y flotantes de 32 y 64 bits. Una función C escribe y lee cada elemento
para inspeccionar órdenes y alineaciones en el IR de Clang. La referencia aún
no se ha generado ni valida su consumo desde NativeSharp.

Clang 23 para `x86_64-pc-windows-msvc`, C11 y O0 generó el IR en
`A:/nsc-interop-build/tanda08-volatile-scalars-reference.ll`. En la función
de referencia, todos los elementos observados usan `store atomic volatile`
con release y `load atomic volatile` con acquire: `_Bool`, signed y unsigned
char usan i8/alineación 1; short usa i16/alineación 2; long long usa
i64/alineación 8; float usa float/alineación 4 y double usa double/alineación
8. Esta evidencia solo cubre esos globales y esa configuración. No prueba
todavía el IR de NSC ni los accesos por punteros o campos.

Se añadió además un arreglo de unsigned int a la referencia para comprobar
explícitamente el caso de 32 bits sin signo; aún falta generar su IR.
