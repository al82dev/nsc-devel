# Congelamiento 3.0 P3 — `where` en un accesor de indexador o de propiedad

Fecha: 2026-10-26. Revisión de partida: `91ac3348b`.

## Problema

P3 pide que las operaciones que necesariamente devuelven una copia exijan `T : copy`, con el mecanismo de P1. El
getter del indexador de `List<T>` devuelve una copia; el setter no copia nada (recibe el valor `owned` y lo
mueve al slot). P1 solo aceptaba `where` en métodos y constructores, así que el indexador no podía expresar la
restricción. Sin ella, `list[i]` sobre `List<Guard>` da desde el doc 25 un NS3020 dentro de `List.ns`, no en
la línea del usuario.

Restringir todo el indexador tampoco sirve: bloquearía `list[i] = move(g)`, que no copia. Lo mismo pasa con una
propiedad cuyo getter copia (el `Current` de un enumerador que arma un par).

## SCOPE LOCK

- **Cambia.**
  - `MemberParser.cpp`, `parseIndexerAccessor` y `parsePropertyAccessor`: después de `throws`, el accesor
    acepta cláusulas `where`. El accesor ya es un nodo de método, así que la regla de P1 (restringir un
    parámetro del tipo que lo contiene) se aplica igual: `get where T : copy { … }`.
  - `ProjectGenericConstraintModel.cpp`: `validateSpecializationMemberClauses` también recorre los accesores
    anidados en un indexador o una propiedad. La comprobación de un miembro pasa a
    `validateSpecializationMemberClause`.
  - `CallAnalyzer.cpp`: `validateSyntheticConstraints`. La llamada sintética que elige el accesor reporta el
    constraint aplazado en el acceso, como lo hace la resolución de una llamada ordinaria.
    El getter que se liga solo para el destino de un `=` simple (la expresión es `activeUnionWriteTarget_`, que
    `analyzeAssignmentLeft` fija solo para `=`) no exige su `where`, porque ese store ejecuta el setter. Si el
    getter devuelve una referencia, el store escribe por ella y el `where` sí aplica.
- **Idéntico.** Los indexadores sin `where`, el `where` de métodos y constructores, el formato de NSMETA (la
  plantilla viaja como fuente) y la caché C1 (el campo `enclosingTypeParameter` ya se serializa por nodo).

## Validación

nsc Release recompilado. Sondas en `A:\temp\nsc-dec\d15`, `-O0` y `-O2`, con
`get where T : copy { return items[i]; } set { items[i] = move(value); }`:

| Sonda | Resultado |
|---|---|
| E1GetCopy (`Box<int>`) | 4500 |
| E2SetMoveOnly (`Box<Guard>`, dos `=` y `RefAt`) | 878: el reemplazo destruye el 7, el final el 8 |
| E3GetMoveOnly (`b[0].Id` con `Box<Guard>`) | «generic parameter 'T' of 'Box.get_Item' requires copyable storage ('where T : copy')» en la línea del acceso |
| E4GetClass (`Box<Payload>`, clase) | 6965: una clase cumple `copy` |
| E6RefGetWhere (indexador `ref` con `get where T : copy`, `=` con `Guard`) | rechazado en cada `=` |
| E7PropCopy (propiedad `get where T : copy`, `Holder<int>`) | 400 |
| E8PropSetMoveOnly (`Holder<Guard>`, dos `=`) | 878 |
| E9PropGetMoveOnly (`h.Value.Id` con `Holder<Guard>`) | «'Holder.get_Value' requires copyable storage», en la línea del acceso |
| XLibApi precompilada; XLibOk (`Box<int>` get, `Box<Guard>` set y `RefAt`) | lib 0; 15 |
| XLibBad (`Box<Guard>` get desde la biblioteca precompilada) | mismo mensaje, en la línea del acceso |

Casos `nsc_component_tests` en Debug y Release: 48 de referencias (96 PASS) y 15 de copia (30 PASS). Corpus de
`await`: mismos resultados que `d04`.

Prueba nueva: `Freeze30ContractTests::testIndexerAccessorWhere` (E1, E2, E3 y la propiedad de E9).
