# T09-003: registros flotantes anidados bajo regcall

En Win64, Clang oficial 23.1.0 expande en tres argumentos `float` tanto un
registro formado por otro registro de dos flotantes y un tercer flotante como
un registro que contiene un arreglo fijo de tres flotantes. Sus retornos LLVM
conservan, respectivamente, las formas `{ { float, float }, float }` y
`{ [3 x float] }`. NSC valida las formas, desplazamientos y atributos del
plan de Clang y emite el tipo de retorno correspondiente, tanto para llamadas
directas como a través de punteros.

La prueba dirigida compara un productor y control escritos en C con llamadas
desde NativeSharp. Pasaron las ocho combinaciones de O0 a O3 y enlace estático
o DLL, incluidos valores de retorno y copia del argumento:
`pruebas-tanda-09-regcall-nested/validar.py`, evidencia en
`A:/temp/nsc-t09-regcall-nested-2op99c0t`. La prueba de planes verifica
también el rechazo de retornos y coerciones alterados. Pasaron las regresiones
de registros planos, cuatro pruebas de sondeo ABI y punteros, comprobaciones
de transporte e identidad y 16 consumidores C++ precompilados de registros
planos.

El consumidor precompilado inicialmente falló al reconstruir el alias de
puntero. La reconstrucción ahora recupera sus registros dependientes del
catálogo vigente, comprueba identidad y layout, los ordena según dependencias
y contrasta las formas reconstruidas con Clang. Pasaron los 16 consumidores
con y sin encabezado, estática y DLL, O0-O3, en
`A:/temp/nsc-t09-hfa34-precompilado-6iocixnf`. También pasaron las
regresiones precompiladas de registros planos `regcall`, de punteros
`regcall` y de registros distintos `sysv_abi`, así como los rechazos de
conversiones incompatibles y las pruebas de transporte e identidad.

Las muestras equivalentes de C++ también pasaron las ocho combinaciones de
O0-O3 y estática/DLL, con llamadas directas y por puntero, comparadas con el
control C++ y las formas LLVM de Clang:
`A:/temp/nsc-t09-cpp-nested-3769ljf_`. Pasaron 16 consumidores
precompilados con ambas llamadas y otros 16 que solo llaman por puntero,
con y sin encabezado: `A:/temp/nsc-t09-cpp-nested-precompilado-5qv8j_p3`
y `A:/temp/nsc-t09-cpp-nested-precompilado-ybdszx6k`.

Para que estas muestras sean accesibles, se proyectan los campos públicos
de registros C++ con operaciones de valor elegibles cuando son arreglos
escalares fijos o registros de valor también elegibles, comprobando su
identidad y layout. La prueba negativa conservó el rechazo de campos privados
y del arreglo de un registro con destructor no trivial:
`A:/temp/nsc-t09-cpp-nested-rechazos-1lo8r471`. Pasaron además regresiones
de registros C, registros planos y metadatos C++.

**Pendiente:** otras firmas, layouts y tipos C++ no triviales no están
acreditados. Este avance no cierra T09-003 ni la tanda 9.

## Firma mixta en investigación

Clang 23.1.0 pasa un registro de tres flotantes seguido de `int` como tres
`float` y un `i32`; el plan directo y el plan por puntero registran offsets
0, 4 y 8 y sitúan el entero en el cuarto parámetro físico. El control C del
fixture `pruebas-tanda-09-regcall-mixed` pasó O0-O3 en estática y DLL
(`A:/temp/nsc-t09-regcall-mixed-kmh6vfxp`). NativeSharp todavía rechaza la
firma en HIR (`A:/temp/nsc-t09-regcall-mixed-l2auwyfn/estatica-O0-llvm.log`).

El validador identifica explícitamente este plan mixto y rechaza las
alteraciones de posición, ancho y atributos comprobadas en la prueba de
planes. Las llamadas C directas con los dos registros anidados ya pasan en
O0-O3, estática y DLL, y se comprueban también las llamadas LLVM y la caché:
`A:/temp/nsc-t09-regcall-mixed-jh3_vser`. Las pruebas anteriores de
registros C y C++ siguen pasando; la prueba de rechazo conserva una firma con
dos enteros adicionales que aún no se admite.

La misma muestra C ahora pasa también por puntero en las ocho configuraciones
(`A:/temp/nsc-t09-regcall-mixed-x1kojva9`). Se amplió la validación del
contrato de NSMeta vigente y la reconstrucción del alias para esta firma:
pasaron 16 consumidores precompilados, con y sin encabezado, en O0-O3 con
estática y DLL (`A:/temp/nsc-t09-hfa34-precompilado-aygqfb6a`). La prueba
que solo usa punteros pasó otras 16 combinaciones
(`A:/temp/nsc-t09-hfa34-precompilado-4uw47wo1`). La prueba de metadatos
confirma la lectura del plan y el rechazo de un tipo alterado.
Pasaron las regresiones de transporte, identidad, firmas anteriores y
rechazos conservados.

La muestra C++ equivalente pasó también en las ocho configuraciones con
llamadas directas e indirectas (`A:/temp/nsc-t09-cpp-mixed-o6sglwcs`).
Pasaron 16 consumidores precompilados con ambas llamadas y otros 16 solo por
puntero, con y sin encabezado
(`A:/temp/nsc-t09-cpp-nested-precompilado-bbg_tme6` y
`A:/temp/nsc-t09-cpp-nested-precompilado-hjnt6p5d`). La prueba del contrato
de alias C++ confirma que el plan sobrevive a la serialización y rechaza el
ancho físico alterado. Se comprobaron también los consumidores C mixtos y los
casos C++ previos.

**Pendiente:** otras combinaciones de argumentos y layouts no están acreditadas.
