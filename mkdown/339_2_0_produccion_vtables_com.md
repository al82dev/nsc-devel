# Ítem 339 — Producción de vtables COM / COM vtable production

## Resultado / Result

NativeSharp certifica la construcción explícita de una interfaz binaria compatible con COM. Un
productor publica las direcciones de sus métodos `extern C` en slots consecutivos, instala el
puntero a la tabla como primer campo del objeto e inicializa su estado. / NativeSharp certifies
explicit construction of a COM-compatible binary interface. A producer publishes its `extern C`
method addresses in consecutive slots, installs the table pointer as the object's first field,
and initializes its state.

## Contrato ABI / ABI contract

La producción no depende de wrappers ni metadatos ocultos: el almacenamiento de la tabla y del
objeto pertenece al llamador, y la rutina de fábrica sólo escribe el layout observable. Los slots
conservan la firma y el primer argumento `self` de la ABI C Win64. / Production depends on no
wrappers or hidden metadata: the caller owns table and object storage, while the factory routine
only writes the observable layout. Slots preserve the signature and first `self` argument of the
Win64 C ABI.

## Pruebas / Tests

La prueba dirigida produce una tabla de tres slots equivalentes a consulta, incremento y
liberación; después la consume exclusivamente desde la memoria publicada. Las variantes O0 y O2
validan identidad de slots, mutación del objeto y resultado final 42. / The focused test produces
a three-slot table equivalent to query, add-reference, and release, then consumes it exclusively
through published memory. O0 and O2 validate slot identity, object mutation, and final result 42.
