# T09: tamaños de subobjeto en llamadas C++ cdecl

Este avance amplía `pass_object_size` y `pass_dynamic_object_size` para los
modos 1 y 3. En las expresiones admitidas se distingue el tamaño del campo o
arreglo del tamaño del objeto que lo contiene: para un arreglo de dos enteros,
el primer elemento deja 8 bytes y el segundo 4. Un índice local entero `const`
se conserva como constante sin repetir ni reordenar una expresión con efectos.

Para una expresión de índice que llama a una función, la variante dinámica de
Clang puede usar el tamaño desconocido, mientras que la variante estática usa
`llvm.objectsize`. El cambio reproduce ese resultado para la forma probada y
evalúa cada argumento una sola vez. Algunas otras expresiones de índice y
tipos de subobjeto todavía se rechazan, porque MIR no conserva suficiente
información para asegurar la misma respuesta que Clang.

## Validación

- Construcción incremental de NSC y pruebas afectadas.
- `pruebas-tanda-09-cpp-tamano-objeto/validar.py --const-parametros`: comparación
  C++/NativeSharp O0–O3 para los cuatro modos y sus variantes dinámicas, con
  locales, campos, arreglos, índices literales y locales constantes. Se compara
  por separado el resultado de cada modo con un índice que llama a una función.
  También se mantiene la prueba de rechazo de conversión a puntero de función.
- `validar-precompilado.py --const-parametros`: ocho consumidores O0–O3, con
  y sin encabezado, incluidos los tamaños de campos y arreglos.
- `validar.py --solo-extremos`: regresión de los modos 0 y 2, O0–O3.
- Pruebas dirigidas de transporte MIR, plan ABI e identidad: las tres pasaron.

Evidencia: `A:/temp/nsc-t09-object-size-6uqwgl_9`,
`A:/temp/nsc-t09-object-size-pre-1o3sd87w` y
`A:/temp/nsc-t09-object-size-m7kfiajw`.

## Límites

No se afirman cubiertas todas las expresiones de índice, arreglos de tipos o
subobjetos, ni las demás convenciones y métodos. También faltan DLL,
sobrecargas y tamaños dinámicos calculados con reservas de tamaño variable.
El ítem T09-003 permanece abierto: esta prueba es evidencia de casos concretos,
no de interoperabilidad general.
