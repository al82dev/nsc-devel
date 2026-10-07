# Tanda universal 09: referencias nativas como aliases

## Scope lock

- Hacer que `ref T` conserve el comportamiento acumulativo de una referencia nativa.
- Permitir varios aliases mutables y acceso directo al almacenamiento mientras viven.
- Conservar lifetime, escape, procedencia, pinning y bloqueo de movimientos del dueno.
- No modificar ownership explicito `owned`/`borrowed`, lowering, ABI ni NSLink.

## Causa confirmada

La comprobacion final de cada expresion consultaba todas las referencias locales vivas y rechazaba
el acceso al almacenamiento original. La declaracion de otra referencia aplicaba la misma regla.
Eso convirtio implicitamente cada `ref` ordinario en un prestamo mutable exclusivo, aunque la
sintaxis no solicitara exclusividad.

El resultado era una contradiccion interna: las referencias historicas eran aliases nativos, pero
el analisis reciente exigia disciplina exclusiva sin una construccion explicita que la eligiera.

## Cambio

1. El acceso a un lugar no se invalida solamente porque exista un `ref` local superpuesto.
2. La creacion de otro `ref` al mismo lugar tampoco se rechaza por exclusividad implicita.
3. Se conserva `referencePlaceKey` y la ascendencia de reborrow.
4. Los movimientos del dueno siguen consultando las referencias vivas y permanecen prohibidos.
5. Los diagnosticos de lifetime, escape, tipo exacto y `pin ref` no cambian.

## Mejora

El programador puede usar referencias con semantica nativa predecible sin activar un sistema de
prestamos exclusivo por omision. La seguridad relacionada con la vida del almacenamiento sigue
siendo comprobable y no depende de una propiedad de version del proyecto.

## Validacion prevista

- Compilar solamente `nsc_item312_reference_locals_tests` y
  `nsc_ns3_t06_reborrow_split_tests`.
- Ejecutar `nsc.item312.reference-locals` para comprobar aliases, lifetime y lowering.
- Ejecutar T06 para medir que expectativas antiguas de exclusividad requieren migracion posterior;
  no se cambiaran en esta tanda.
- No ejecutar baseline, instalar ni hacer push.
