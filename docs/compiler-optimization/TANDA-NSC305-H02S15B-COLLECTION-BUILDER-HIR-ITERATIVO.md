# H02S15B - elementos de collection builder HIR iterativos

## SCOPE LOCK

- Objetivo único: retirar la reentrada `CollectionBuilder -> lowerExpression(element)`.
- Base: `1af9f28e98d4f44dd2e62338a5e3d4a09027d5ee`.
- Contratos congelados: `CreateBuilder -> Add* -> ToCollection`, orden fuente, forma HIR,
  member ordinals, ordinal sintético preorder, diagnósticos, ownership, ABI y NSMETA.
- Protegido: arrays/S15C, aggregate/tuple/with/S15F, parser, Sema, MIR, LLVM y tracking global.
- Validación única: target `nsc_project_oop_lowering_tests`, selector
  `hir-collection-builder-depth`.

## Implementación

`CollectionBuilder` es ahora una variante del scheduler común. El frame reserva exactamente un
ordinal sintético antes de descender, conserva el `memberOrdinal` consumidor, agenda un elemento
por reanudación en value context y acumula los HIR en orden fuente.

`finishCollectionBuilderExpression` es el finalizador único: recibe elementos ya bajados y emite
la declaración con `CreateBuilder`, un `ExpressionStatement(Add)` por elemento y el
`ToCollection` final sin wrapper. No llama a ningún lowerer. El builder vacío conserva la ruta
histórica y usa el mismo finalizador. Un builder no vacío que escape del scheduler falla con una
guarda interna, sin reabrir la recursión.

## Prueba focal y evidencia

El selector construye 32,768 `CollectionExpression` anidadas, un elemento por nivel, y entra por
`lowerExpression`. El comprobador iterativo valida en cada nivel:

- `BlockExpression` con tres hijos;
- declaración, `Add` y `ToCollection` en el orden histórico;
- member ordinals `31`, `32`, `33`;
- receivers `LocalAddress` con el mismo ordinal que la declaración;
- ordinales sintéticos preorder desde cero;
- elemento anidado exactamente en `Add.children[1]`;
- literal terminal intacto.

La hoja terminal usa binding `builtin:int`; sólo cada `CollectionExpression` recibe el binding de
la colección. El mismo selector valida además builder vacío, tres elementos distinguibles en orden
fuente, builders hermanos/anidados con ordinales preorder, rechazo address sin consumir ordinal,
fallo interno con diagnóstico/rango del elemento y ausencia de bloque parcial, y una
`CollectionExpression` con core `ArrayAllocation` que conserva la ruta legacy fuera del frame.
Las aserciones cubren `syntax` y `typeKey` de
declaration/create/statement/add/toCollection y de ambos receivers.

- Build final exacto: `A:\NSC-devel-paralelo\logs\agente-1\h02s15b-build-amend.log`.
- Selector final exacto: `A:\NSC-devel-paralelo\logs\agente-1\h02s15b-focus-amend.log`.
- Resultado: `PASS: 8 checks`.
- No se ejecutó CTest, suite, minibaseline, baseline ni otro selector.

## Auditoría de no reentrada

La ruta ejercitada es `iterativeKind(CollectionBuilder) -> frame común -> un elemento por
reanudación -> finishCollectionBuilderExpression`. El finalizador y la rama histórica builder ya
no contienen `lowerExpression(element)`. La profundidad reside en el vector de frames del heap,
no en la pila nativa.
