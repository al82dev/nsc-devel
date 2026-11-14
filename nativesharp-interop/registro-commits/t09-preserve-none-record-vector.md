# T09: record C++ con vector en retorno preserve_none

## Cambio

La muestra `Small { float4 values; double wide; }` era rechazada al solicitar el plan `preserve_none`. NSC ahora copia los vectores fijos escalares anidados en agregados del plan Clang y calcula su tamaño y alineación para el data layout Win64 verificado. Se mantienen las guardas anteriores para vectores directos y tipos no representados. Al reconstruir records C++ precompilados, un campo de tipo no soportado se deja inaccesible, como en la importación viva, conservando el layout verificado para las copias. La ruta C no se amplió y no se modifica NSMeta.

## Validación

- El control Clang de la firma y sus helpers pasó estática y DLL O0–O3: `A:/temp/nsc-t09-preserve-cpp-retorno-record-vector-78j2bzxl`. Clang observó `{<4 x float>, double}` y retorno indirecto alineado a 16: `A:/temp/t09-preserve-record-vector-control.ll`.
- Ocho comparaciones Clang/NativeSharp directas y por callback comprueban los cuatro elementos, el `double` y que el original no cambie: `A:/temp/nsc-t09-preserve-cpp-retorno-record-vector-wrsqzo5r`.
- Dieciséis consumidores precompilados con y sin encabezado, además del rechazo del acceso al campo vectorial sin encabezado: `A:/temp/nsc-t09-preserve-cpp-retorno-record-vector-precompilado-ll1oonat`. La ruta viva rechaza el mismo acceso: `A:/temp/nsc-t09-vector-campo-oculto-itp74dun`.
- `validar-layout.py` contrastó tamaños y alineaciones de vectores fijos representativos con Clang: `A:/temp/nsc-t09-vector-layout-eozubfsg`. Las tres pruebas internas ABI, observación y plan terminaron con código 0: `A:/temp/t09-scoped-enum-unit-tests.log`. La prueba de rechazo de vector directo se conservó intacta.
- Regresiones precompiladas de record con arreglo, enum más record SysV y record C: `A:/temp/nsc-t09-preserve-cpp-retorno-record-arreglo-precompilado-cp4fiyr5`, `A:/temp/nsc-t09-cpp-variadica-sysv-enum-record-precompilado-w3w3r208` y `A:/temp/nsc-t09-sysv-indirecta-record-pre-1kg4wwl9`.

## Límites

Se acredita esta firma y este record en Windows x64, no el uso directo de campos vectoriales, vectores escalables, vectores directos en firmas ni todas las clases de vector o layout. El cálculo de layout está acotado y se usa con el data layout Win64 comprobado. T09-003 y T11 permanecen abiertos.
