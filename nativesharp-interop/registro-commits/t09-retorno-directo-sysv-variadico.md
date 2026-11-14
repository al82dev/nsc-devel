# T09: retorno directo de agregado en llamada variádica SysV

La declaración y la llamada variádicas SysV utilizan el plan físico de Clang
para devolver records triviales en registros. MIR conserva las posiciones
físicas del plan base y LLVM reconstruye el valor a partir de las piezas que
recibe, verificando que cada pieza quepa en el layout del record. El contrato
precompilado conserva la identidad y el layout observados por Clang.

## Validación

- Compilación incremental y comprobaciones dirigidas de plan ABI, identidad y
  transporte MIR. La prueba interna comprueba que la identidad HIR cambia al
  cambiar el plan base y hace una ida y vuelta real por una caché MIR
  habilitada, verificando que el plan se recupera desde HIR.
- `pruebas-tanda-09-cpp-variadica-sysv-record/validar-retorno-directo.py`:
  retorno mixto `{double,int}`, una pieza entera de 64 bits y dos enteros de
  64 bits, dos flotantes devueltos como vector y una pareja de piezas
  vectorial/`double`, todos con agregado opcional. También se verificaron los
  retornos mixto y vectorial/`double` con agregado fijo de entrada y un
  argumento variádico entero. Cada variante compara ocho llamadas C++/NativeSharp y
  dieciséis consumidores precompilados con y sin encabezado, estática y DLL
  O0–O3. Se comprueba la firma física y, para retornos de dos piezas, su
  extracción en IR.
- `pruebas-tanda-09-c-sysv-variadica-tamano/validar-retorno-directo.py`:
  retorno mixto C, agregado opcional y tamaño oculto. Ocho controles C, ocho
  llamadas NativeSharp y dieciséis consumidores precompilados; se comprueba la
  firma y extracción en IR.
- Regresiones de retornos indirectos y agregados fijos C y C++, incluyendo
  precompilación.

Registros y rutas de evidencia: `A:/temp/t09-direct-return-final` y
`A:/temp/t09-direct-return-cache-final` y
`A:/temp/t09-direct-return-single.log`, `A:/temp/t09-direct-vector-final`,
`A:/temp/t09-direct-fixed.log` y `A:/temp/t09-direct-fixed-vector.log`.

## Límites

Las muestras no certifican todas las composiciones, alineaciones, tipos ni
convenciones. Algunas coerciones o layouts fuera de las piezas verificadas se
rechazan; la reconstrucción sin encabezado se limita a records simples cuya
identidad y campos pueden verificarse. T09-003 continúa abierto.
