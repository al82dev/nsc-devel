# Validación dirigida de campos de bits C

Se construyó incrementalmente el compilador experimental en `A:/nsc-interop-build/tanda1-ON` y se ejecutó `nativesharp-interop/pruebas-tanda-08-bitfields-c/validar.cmd`. En O0, O1, O2 y O3, código C inicializó los structs y NativeSharp leyó y modificó los campos; después C comprobó los valores. Se cubrieron campos enteros con y sin signo, separación mediante anchura cero y un struct empaquetado con alineación uno.

La prueba es dirigida y no cubre todos los campos de bits C: unions, accesos volatile, persistencia en metadata precompilada y otras combinaciones continúan pendientes. La tanda 08 permanece abierta.
