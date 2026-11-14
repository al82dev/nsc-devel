# T09: agregado fijo en llamada variádica SysV

Se obtiene de Clang un plan base de la firma variádica y se transporta desde
HIR a MIR. LLVM declara la función con los parámetros físicos de ese plan y
comprueba que cada llamada coincida. Los atributos de extensión y `byval` se
aplican en posiciones físicas; la identidad de caché incluye el plan base.
En el consumo precompilado, el agregado fijo se reconstruye con su nombre y
espacio de nombres originales. Se contrastan identidad, campos, layout,
símbolo y plan con los datos capturados y con Clang.

## Validación

- Compilación incremental del compilador de desarrollo.
- `pruebas-tanda-09-cpp-variadica-sysv-record/validar-agregado-fijo.py`:
  agregado fijo mixto `double`/`int`, puntero con tamaño oculto y argumento
  variádico entero; variante con agregado variádico adicional; variante con
  `signed char` y `bool` fijos después del agregado, verificando `signext` y
  `zeroext` en declaración y llamada. Para cada una, ocho comparaciones C++ y
  NativeSharp y dieciséis consumidores precompilados con y sin encabezado,
  en estática y DLL, O0–O3.
- Regresiones dirigidas de agregados y tamaños ocultos en C y C++, plan ABI,
  identidad y transporte MIR. La enumeración C++ variádica SysV se repitió
  directa y precompilada. Registros en `A:/temp/t09-fixed-final-checks`,
  `A:/temp/t09-fixed-enum-regression` y `A:/temp/t09-fixed-guard-final`.

Evidencia principal: `A:/temp/nsc-t09-fixed-record-size-w41g1c8e`,
`A:/temp/nsc-t09-fixed-record-size-xlitztu3` y
`A:/temp/nsc-t09-fixed-record-size-j37ydte8`.

## Límites

Se probaron las firmas indicadas, no todas las combinaciones de agregados,
atributos, tipos y convenciones. La reconstrucción precompilada de agregados
fijos está limitada a records C++ simples y verificables en el catálogo;
no acredita records C fijos, agregados indirectos ni resultados indirectos.
T09-003 permanece abierto.
