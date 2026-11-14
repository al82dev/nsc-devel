# Inferencia, alias, delegados, capturas, eventos y propiedad de arreglos

Fichas LKF-083, 085, 086, 087, 088, 090, 092, 093, 095, 096, 097 y 098.

## Defectos corregidos

- **LKF-096.** Un spread de un arreglo nulo (`[..a, 1]` con `a == null`) leía la dirección cero
  (`0xC0000005`). `collectArrayAllocationCount` guarda ahora el origen del spread con
  `checkedReceiver`, que atrapa con 135 como cualquier otro acceso a un arreglo nulo.
- **LKF-097.** `values[i] = values[i] + 1` sobre una VLA se rechazaba como escape: "pointer into
  variable-length array … cannot be stored outside a local alias". La regla miraba cualquier mención
  de la VLA en el valor asignado. Ahora solo aplica cuando el valor es un puntero; guardar
  `&values[0]` en un estático se sigue rechazando.
- **LKF-095.** `nsc.ns3.t32.range-arrays` esperaba que `delete[]` de un `Payload[,]` sin calificar
  destruyera sus elementos. Por la decisión de `b45a64498`, un arreglo plano no posee sus elementos y
  uno `owned` sí, en una y en varias dimensiones. La prueba declara ahora `owned Payload[,]` y pasa.

## Verificado sin cambio

- **LKF-083.** La inferencia usa el tipo destino: retorno solo, `null` y lambda. La evidencia
  incompatible o ausente se rechaza ("cannot be inferred … write them explicitly").
- **LKF-085.** Un alias genérico aplica las restricciones del tipo subyacente y no admite `where`
  propio (error de sintaxis).
- **LKF-086.** Un método virtual como destino de delegado despacha el override. Un receptor `struct`
  se rechaza con diagnóstico explícito. Un parámetro genérico como retorno de delegado funciona.
- **LKF-087.** Capturar un local `raii`, un parámetro `owned` o una VLA se rechaza indicando la
  alternativa; `[move x]` y la captura por valor ejecutan. La parte de callbacks C queda fuera de
  alcance.
- **LKF-088.** Los manejadores de eventos se invocan en orden de suscripción (FIFO), como los
  delegados multicast de C#. El capítulo histórico que describe LIFO está desactualizado.
- **LKF-090.** `delete[]` de un arreglo `owned` destruye sus elementos; el de uno plano no los toca.
- **LKF-092.** Mover el dueño de un arreglo con un préstamo vivo se rechaza; cuando el préstamo ya
  terminó, se mueve.
- **LKF-093.** Un arreglo inline no se pasa ni se devuelve por valor, con diagnóstico y alternativa;
  dentro de un struct pasado por `ref` funciona.
- **LKF-098.** La cola flexible no tiene límites automáticos: su longitud es externa y es nivel crudo
  por diseño.

## Prueba

`nsc.compiler.generic-delegate-array-boundaries`: 29 casos, cada rechazo con su control, O0/O2.
`nsc.ns3.t32.range-arrays` pasa.
