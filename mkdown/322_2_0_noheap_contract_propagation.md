# 322 — Propagación de contratos `noheap` / No-heap contract propagation

## Resultado / Outcome

`noheap` deja de ser solamente una comprobación local. Un método o constructor con ese modificador
sólo puede invocar destinos que publiquen la misma garantía. La regla incluye llamadas directas,
constructores placement o de valor, inicializadores `base(...)`/`this(...)` e inicializadores de
campo que forman parte del cuerpo efectivo de un constructor.

The compiler now verifies the complete statically visible no-heap call graph instead of accepting
an unannotated callee behind an allocation-free call expression.

## Dispatch e indirección / Dispatch and indirection

Un override no puede eliminar `noheap` de su ranura base y una implementación de interfaz debe
conservar el contrato publicado. Así, llamar mediante una referencia base o de interfaz no cambia
la garantía según el tipo dinámico. Los delegates y eventos aún no expresan efectos en su tipo;
por eso su invocación se rechaza dentro de `noheap` en lugar de asumir que el destino es seguro.

Placement construction sigue sin reservar memoria, pero el constructor que ejecuta debe ser
explícito y `noheap`. El constructor implícito no tiene un sitio donde declarar la garantía y se
rechaza conservadoramente.

## Representación y compatibilidad / Representation and compatibility

HIR y MIR conservan `isNoHeap` en cada función; el schema MIR avanza de 1.4 a 1.5. NSMETA 1.16
reserva el bit 25 de un miembro callable, lo valida, lo reconstruye en `LibraryReference` y lo
elimina cuando se solicita una salida histórica 1.15. No cambian el ABI nativo, Object ABI, layout,
runtime ni bindings.

## Cobertura / Coverage

`tests/unit/Item322NoHeapPropagationTests.cpp` cubre cadenas válidas, llamadas ordinarias
inválidas, constructores placement explícitos e implícitos, inicializadores base y de campo,
override, interfaz, delegate, HIR/MIR 1.5 y round-trip/downlevel/consumo real de NSMETA 1.16.

## Mensaje de commit / Commit message

`Propaga contratos noheap / Propagate no-heap contracts`
