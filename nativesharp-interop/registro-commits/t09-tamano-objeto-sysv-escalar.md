# T09: tamaños de objeto y convención SysV para escalares

Se habilitan argumentos ocultos `pass_object_size` y
`pass_dynamic_object_size` en funciones libres no variádicas `sysv_abi` cuyas
firmas son escalares o de punteros en Windows x64. Se mantiene el rechazo para
firmas SysV con agregados y variádicas hasta representar y comprobar sus planes
físicos completos.

También se corrige la posición de los atributos LLVM `zeroext` y `signext`
cuando hay tamaños ocultos antes de parámetros booleanos o enteros estrechos.
Antes, por ejemplo, `zeroext` podía aplicarse al `ushort` anterior en lugar de
al booleano. Se probó la posición en la declaración y en la llamada con uno y
dos tamaños ocultos, además de contrastar la ejecución con Clang.

## Validación

- Construcción incremental de NSC y pruebas dirigidas de transporte, identidad
  y plan ABI.
- `pruebas-tanda-09-cpp-tamano-objeto/validar.py --sysv --const-parametros`,
  con y sin `--dll`: comparación directa O0–O3 para cuatro modos, sus variantes
  dinámicas, índices literales y variables. Las pruebas revisan además los
  atributos físicos de booleanos y enteros estrechos.
- `validar-precompilado.py --sysv --const-parametros`, con y sin `--dll`:
  consumidores con y sin encabezado, O0–O3. El caso con DLL incluye dos tamaños
  ocultos antes de parámetros estrechos.
- `pruebas-tanda-09-c-tamano-objeto/validar.py --sysv` y
  `validar-precompilado.py --sysv`: llamadas y consumidores C, en estática y
  DLL, O0–O3 y con o sin encabezado para la precompilación.
- Regresiones cdecl C y C++, incluida la posición del atributo booleano con
  uno y dos tamaños ocultos.

Evidencia: `A:/temp/nsc-t09-object-size-denieduq`,
`A:/temp/nsc-t09-object-size-b38ss9jm`, `A:/temp/nsc-t09-object-size-mezgnaa_`,
`A:/temp/nsc-t09-object-size-pre-p4b2ru7e`, `A:/temp/nsc-t09-c-object-size-kjnvw4xn`
y `A:/temp/nsc-t09-c-object-size-pre-9_yc26b9`.

## Límites

No se consideran cubiertas las firmas SysV con agregados o variádicas, otras
convenciones, métodos, sobrecargas ni todas las expresiones de tamaño. El
ítem T09-003 sigue abierto; las pruebas acreditan únicamente las formas
descritas.
