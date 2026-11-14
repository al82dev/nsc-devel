# T09: retorno indirecto de agregado en llamada variádica SysV

Las llamadas variádicas SysV pueden devolver indirectamente un record C o C++
cuando el plan de Clang y el tipo importado acreditan identidad, layout y
operaciones de valor. El plan base se transporta hasta LLVM; la llamada reserva
el resultado, coloca el puntero de retorno en la posición física indicada,
comprueba la alineación y recupera el valor después de la llamada. El atributo
`sret` se conserva en la declaración y en la llamada. La reconstrucción
precompilada verifica de nuevo la identidad y los campos del tipo de retorno.

## Validación

- Compilación incremental del compilador de desarrollo y pruebas dirigidas del
  plan ABI, identidad y transporte MIR.
- `pruebas-tanda-09-cpp-variadica-sysv-record/validar-retorno-indirecto.py`:
  record C++ de 24 bytes devuelto por `sret`, con agregado opcional; variantes
  con argumentos opcionales escalares, sin argumentos opcionales y con tamaño
  de objeto oculto. Cada variante contrasta ocho llamadas de C++ y NativeSharp y dieciséis
  consumidores precompilados con y sin encabezado, estática y DLL O0–O3.
  La prueba comprueba `sret` y alineación en declaración y llamada.
- `pruebas-tanda-09-c-sysv-variadica-tamano/validar-retorno-indirecto.py`:
  record C de 24 bytes, tamaño oculto y agregado opcional; ocho controles C,
  ocho llamadas directas y dieciséis consumidores precompilados en las mismas
  configuraciones, con comprobación de `sret` en IR.
- Regresiones de agregados fijos C y C++ y sus consumidores precompilados.

Los resultados y rutas de evidencia se conservan en `A:/temp/t09-sret-final`
y `A:/temp/t09-sret-empty-final.log`.

## Límites

Las muestras no certifican todos los records, atributos o convenciones. Los
resultados de agregados pasados directamente en registros no quedan acreditados
por estas pruebas. La reconstrucción sin encabezado admite los records simples
que el catálogo y Clang pueden verificar. T09-003 continúa abierto.
