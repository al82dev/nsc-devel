# Tanda universal 30 - Compatibilidad acumulativa de BorrowedReference

## Causa confirmada

La prueba T31 no fallaba por resolución genérica ni por las reglas de préstamo. El commit
`9232bcfc0` eliminó `lib/NativeSharp.Core/BorrowedReference.ns` al sincronizar la copia incluida
de Core, mientras la prueba histórica y la documentación pública conservaron el contrato.

## Cambio

Se restaura `BorrowedReference<T>` como `borrowed struct` de una sola referencia y `Borrows.Of<T>`
como constructor de la vista. La vista no tiene destructor, no destruye el objeto prestado y no
realiza asignaciones.

## Por qué no se modificó el compilador

El compilador ya reconoce y verifica este contrato. Debilitar name lookup o fabricar el símbolo
ausente habría ocultado una pérdida de superficie pública. La corrección pertenece al productor
del símbolo dentro de la copia de Core usada por las pruebas.

## Validación prevista

1. Ejecutar `nsc.ns3.t31.array-borrow` con O0/NSLink.
2. Ejecutar la batería CTest completa con el entorno explícito usado en la tanda 29.
