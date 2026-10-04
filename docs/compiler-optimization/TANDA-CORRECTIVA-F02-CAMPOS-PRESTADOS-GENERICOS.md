# Correctiva F02: campos prestados dentro de tipos genericos

## Problema

La implementacion de `NS3-T06-F02` clasificaba como destino de transferencia de propiedad a
cualquier campo de un metodo perteneciente a una especializacion generica. Eso convertia por
error campos de referencia ordinarios y prestados, como `List<T> source`, en campos `owned`.

El primer diagnostico aparecia en
`lib/NativeSharp.Core.Collections.Generic/ListEnumerator.ns`; los restantes fallos del reporte
eran cierres derivados porque el compilador self-hosted y varias pruebas consumen esa biblioteca.

## Cambio

- La asignacion a un campo exige transferencia solamente cuando el campo fue declarado `owned`,
  su tipo es move-only o su tipo declarado es un parametro generico no restringido.
- La genericidad del tipo contenedor ya no convierte todos sus campos en almacenamiento owned.
- Se agrego una regresion ejecutable para una clase generica que conserva una referencia
  prestada y la inicializa mediante asignacion ordinaria.

## Archivos

- `src/Frontend/ProjectMethodBodyModel.cpp`
- `tests/unit/NativeSharp3T06PartialMoveTests.cpp`

## Verificacion puntual

Se construye y ejecuta unicamente `nsc_ns3_t06_partial_move_tests` en O0 mediante NSLink. No se
ejecuta baseline ni una coleccion amplia de pruebas.
