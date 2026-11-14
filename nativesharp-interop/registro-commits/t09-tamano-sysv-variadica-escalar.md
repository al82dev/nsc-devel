# T09: tamaño oculto en funciones SysV variádicas escalares

Se permite que una función libre `sysv_abi` variádica con parámetros fijos
escalares o punteros reciba un argumento oculto de tamaño de objeto. La ruta
ya existente de llamadas escalares coloca ese tamaño después del puntero y
conserva las promociones de los argumentos variables.

## Validación

- Compilación incremental de NSC y pruebas de transporte, identidad y plan ABI.
- `pruebas-tanda-09-cpp-variadica-sysv/validar-tamano-objeto.py`: receptor C++
  compilado por Clang comprueba registros enteros, tamaño oculto, promoción a
  `double` y registro del número de argumentos flotantes. Se probaron el modo 0,
  el modo 1 y la variante dinámica modo 3. Con `--fijos-estrechos` también se
  comprobaron un `signed char` y un booleano fijos después del tamaño, incluida
  la posición de `signext` y `zeroext` en el IR.
- Para cada variante ejecutada: ocho comparaciones directas C++/NativeSharp en
  estática y DLL O0–O3 y dieciséis consumidores precompilados con y sin
  encabezado. El caso con agregado opcional se verifica como rechazo pendiente.
- `pruebas-tanda-09-c-sysv-variadica-tamano/validar.py`: ocho controles C,
  ocho consumidores directos y dieciséis precompilados, estática y DLL O0–O3;
  se verifica también la firma y los atributos de la llamada.
- Regresión existente `validar-noescape.py`: ocho comparaciones y dieciséis
  consumidores precompilados pasaron.

Evidencia: `A:/temp/nsc-t09-variadic-object-size-l88c4rqi`,
`A:/temp/nsc-t09-variadic-object-size-6u70z0bz`,
`A:/temp/nsc-t09-variadic-object-size-favcytdo`,
`A:/temp/nsc-t09-variadic-object-size-pbxx3wrj`,
`A:/temp/nsc-t09-c-sysv-var-size-4ipsxd2d` y
`A:/temp/nsc-t09-variadic-noescape-8xjcwpsp`.

## Límites

El plan variádico que incluye agregados aún rechaza `pass_object_size`, por lo
que esas llamadas permanecen pendientes. Tampoco se afirman probadas todas las
formas de argumentos, otras convenciones o métodos. T09-003 sigue abierto.
