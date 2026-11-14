# T09: agregado variádico SysV con tamaño oculto

El plan de una llamada variádica conserva ahora qué parámetros fijos tienen un
`size_t` oculto. La clasificación consulta a Clang con ese argumento en su
posición real; el backend valida la correspondencia con la declaración antes
de emitirlo. El plan y la información de la declaración se conservan en el
contrato actual de metadatos, y la reproducción sin encabezado reconstruye el
atributo y los calificadores del puntero. Para los parámetros booleanos C++
se reconstruye su escritura de C++ desde el tipo canónico del catálogo, y se
comprueban de nuevo la identidad y el plan observados por Clang. El cálculo
reutiliza la expresión ya
evaluada y respeta los mismos límites de tamaños de objeto que las llamadas
escalares.

## Validación

- Construcción incremental; pruebas dirigidas de plan ABI, identidad,
  transporte MIR y observación de records. La prueba del plan verifica la
  serialización, el tamaño físico oculto, entradas truncadas o inválidas y la
  clasificación cuando faltan registros.
- `pruebas-tanda-09-cpp-variadica-sysv-record/validar-tamano-objeto.py`:
  agregado mixto de `double` e `int`, con un tamaño oculto; también con
  `--dos-punteros --const-parametros` y con `--modo 3 --dinamico
  --const-parametros` y `--fijos-estrechos` (con `signed char` y `bool` fijos).
  Cada ejecución compara C++ y NativeSharp en estática y
  DLL O0–O3 y comprueba dieciséis consumidores precompilados con y sin header.
- `validar-tamano-presion.py`: cuatro enteros fijos, puntero y tamaño oculto
  agotan los registros enteros. Un receptor de Clang comprueba el agregado de
  dos enteros de 64 bits en pila; se comprueba `byval` en el IR. Pasan ocho
  comparaciones directas y dieciséis consumidores precompilados.
- `pruebas-tanda-09-c-sysv-variadica-tamano/validar-agregado.py`: ocho controles
  C, ocho llamadas directas y dieciséis consumidores precompilados en estática
  y DLL O0–O3.
- Regresiones: agregado SysV variádico sin tamaño oculto, llamada escalar con
  parámetros fijos estrechos y sus consumidores precompilados.

Evidencia: `A:/temp/nsc-t09-variadic-record-size-wa2w8qh0`,
`A:/temp/nsc-t09-variadic-record-size-f0t3571z`,
`A:/temp/nsc-t09-variadic-record-size-vjbb37po`,
`A:/temp/nsc-t09-variadic-record-size-3z8gb2l_`,
`A:/temp/nsc-t09-variadic-size-pressure-im3va573`,
`A:/temp/nsc-t09-c-sysv-var-record-size-jescld_b` y
`A:/temp/nsc-t09-variadic-object-size-1zwiqfsn`.

## Límites

Las muestras no cubren todas las combinaciones de agregados, tamaños ocultos,
atributos, expresiones ni convenciones. Los parámetros fijos que son agregados
y los resultados indirectos con este atributo no quedan acreditados por estas
pruebas. T09-003 permanece abierto.
