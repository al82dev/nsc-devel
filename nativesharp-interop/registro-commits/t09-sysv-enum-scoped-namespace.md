# SysV: enum class C++ en namespace

Tras los cambios de tipo scoped y catálogo, se contrastó un record trivial cuyo
campo es `T09Cpp::Mode`, declarado como `enum class Mode : int`. La función
SysV recibe el record por copia y devuelve un record mixto pequeño; la muestra
usa el enumerador calificado y llamadas directa y por puntero.

`pruebas-tanda-09-sysv-cpp-copia-enum-scoped-namespace/validar.py` pasó ocho
comparaciones de Clang 23.1.0 y NativeSharp, en estática y DLL O0–O3:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-3zvgl0rh`. Su prueba
`validar-precompilado.py` pasó 16 consumidores con y sin encabezado:
`A:/temp/nsc-t09-sysv-cpp-copia-relleno-precompilado-w9mafz1d`.

No hizo falta cambiar código para esta combinación. La evidencia corresponde
a un enum y firma concretos; otras bases y formas siguen pendientes. T09-003
permanece abierto.
