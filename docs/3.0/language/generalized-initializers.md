# Inicializadores generalizados / Generalized initializers

## Español

NativeSharp 3 acepta inicializadores con valores posicionales, miembros nombrados y un único
spread. Los miembros se evalúan en el orden textual de la fuente, aunque sus destinos estén en
otro orden físico. Un spread se evalúa una vez y sólo completa los slots que no fueron escritos
explícitamente.

```nativesharp
Pair baseValue = { 10, 32 };
Pair value = { Y = MakeY(), X = MakeX() };
Pair changed = { X = 20, ..baseValue };
int[3] copy = { ..source };
```

Las formas generalizadas deben inicializar todos los miembros. La única forma de omitir un
miembro es proporcionar un spread explícito que lo complete. La forma posicional histórica
conserva el zero-fill de los slots restantes.

Los destinos productivos son structs, structs genéricos cerrados, tuplas y colecciones estáticas
`T[N]`. Para `T[N]`, el tipo de elemento y `Count` forman un protocolo intrínseco resuelto en
compilación: no crece el contenedor, no se asigna heap y un exceso de elementos es diagnóstico.
Las colecciones de usuario se incorporarán mediante traits nominales; el compilador no reserva un
método mágico `Add` o `Create`.

Los campos `readonly` pueden escribirse durante la construcción del valor. Después de completarse
el initializer conservan su inmutabilidad ordinaria. Los nombres desconocidos, duplicados, spreads
múltiples y spreads de otro tipo exacto se rechazan antes de HIR.

Los structs no pueden declarar destructor. Un struct con campos `owned` tampoco puede usar una
forma generalizada o spread: hasta que T06 aporte drop flags por campo, el compilador lo rechaza en
vez de filtrar un campo ya construido si falla una expresión posterior. Debe usarse un constructor,
que es la ruta con cleanup definido.

Un initializer de local escribe directamente en el slot final. HIR conserva `aggregate-spread` y
MIR `aggregate-spread-store`; no se crea un temporal `aggregate.init` para luego copiarlo. Los
temporales sólo permanecen cuando el valor aparece como argumento, retorno o subexpresión y no hay
un destino final suministrado por el contexto.

## English

NativeSharp 3 accepts positional values, named members, and one spread in an initializer. Entries
are evaluated in source order even when their physical destinations use a different order. A
spread is evaluated once and fills only slots not written explicitly.

Generalized forms must initialize every member. A member may be omitted only when an explicit
spread supplies it. The historical positional form retains zero-fill for trailing slots.

Productive targets are structs, closed generic structs, tuples, and statically sized `T[N]`
collections. For `T[N]`, element type and `Count` form an intrinsic compile-time protocol: the
container cannot grow, no heap allocation is implied, and excess elements are diagnosed. User
collections will adopt nominal traits; the compiler does not reserve a magic `Add` or `Create`
method.

`readonly` fields may be written while the value is being constructed and remain immutable after
completion. Unknown or duplicate names, multiple spreads, and a spread whose exact type differs
are rejected before HIR.

Structs cannot declare destructors. A struct containing `owned` fields also cannot use a
generalized form or spread. Until T06 provides per-field drop flags, the compiler rejects that
operation instead of leaking an earlier field when a later expression fails. A cleanup-aware
constructor is the supported alternative.

A local initializer writes directly into its final slot. HIR retains `aggregate-spread` and MIR
retains `aggregate-spread-store`; LLVM does not create and copy an `aggregate.init` temporary for
locals. A temporary remains only for arguments, return values, and subexpressions without a final
destination supplied by their context.
