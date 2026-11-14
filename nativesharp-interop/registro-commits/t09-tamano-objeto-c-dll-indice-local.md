# T09: tamaño de objeto en C, DLL e índice local variable

Tras conservar en HIR los índices `const` cuyo valor ya se conoce, el backend
puede tratar un índice local no plegado como variable. Los modos 1 y 3 usan
las mismas reglas ya comprobadas para índices variables; se mantiene la
distinción de la variante dinámica cuando el índice tiene efectos laterales.

## Validación

- Construcción incremental de NSC y pruebas afectadas.
- `pruebas-tanda-09-cpp-tamano-objeto/validar.py --const-parametros`, con y sin
  `--dll`: control C++ y consumidor NativeSharp en O0–O3. Además se comparan
  por separado los ocho modos/variantes para índices que llaman a una función
  y para índices locales inicializados mediante esa llamada. En la muestra el
  modo dinámico 1 recibe 16 y el estático 1 recibe tamaño desconocido.
- `validar-precompilado.py --const-parametros`, con y sin `--dll`: dieciséis
  consumidores en total, con y sin encabezado, también ejercitan índice local.
- `pruebas-tanda-09-c-tamano-objeto/validar.py`: ocho comparaciones C/NativeSharp,
  estática y DLL en O0–O3, para los cuatro modos y las variantes dinámicas.
- `pruebas-tanda-09-c-tamano-objeto/validar-precompilado.py`: dieciséis
  consumidores de bibliotecas C estáticas y DLL, con y sin encabezado.
- Pruebas dirigidas de transporte MIR, planes ABI e identidad: las tres pasaron.

Evidencia: `A:/temp/nsc-t09-object-size-m3dwh_9b`,
`A:/temp/nsc-t09-object-size-l302drka`, `A:/temp/nsc-t09-object-size-pre-or55418k`,
`A:/temp/nsc-t09-object-size-pre-0cgbsnq0`, `A:/temp/nsc-t09-c-object-size-gjc7_ha9`
y `A:/temp/nsc-t09-c-object-size-pre-u5uzrhh0`.

## Límites

Estos controles usan Windows x64 y funciones libres cdecl. Permanecen abiertos
otros tipos y expresiones de subobjeto, convenciones, métodos, sobrecargas y
reservas con tamaños variables. Las pruebas son evidencia de estos casos y no
cierran T09-003 ni acreditan interoperabilidad general.
