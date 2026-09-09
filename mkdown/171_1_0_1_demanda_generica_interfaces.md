# 171 — Demanda genérica desde tablas de interfaz / Generic demand from interface tables

## Resultado / Outcome

Los métodos concretos clonados desde un tipo genérico cerrado ya alcanzan HIR/MIR cuando su único
uso ejecutable es ocupar un slot de interfaz. El compilador deja de fallar con:

```text
nsc: error: Stage 36 concrete interface implementation has no OOP HIR function
```

Concrete methods cloned from a closed generic type now reach HIR/MIR when their only executable
use is filling an interface slot. The compiler no longer fails with the Stage 36 message above.

## Causa / Cause

Stage 44 materializa de forma eager las firmas de `Box<int>`, pero difiere los cuerpos ordinarios
hasta que existe demanda. `ProjectMethodBodyModel` sólo sembraba demanda desde llamadas directas y
conversiones a delegate. Una llamada mediante `IValue` selecciona el miembro contrato de la
interfaz; la implementación concreta `Box<int>.Get` sólo aparece en
`ClassInterfaceTablePlan::implementationMemberOrdinals`. Por tanto, el plan Stage 36 conocía el
slot concreto, pero `ProjectOopHirModel::functionForMember` no tenía función para él.

Stage 44 eagerly materializes `Box<int>` signatures but defers ordinary bodies until demanded.
`ProjectMethodBodyModel` only seeded demand from direct calls and delegate conversions. A call
through `IValue` selects the interface contract member; concrete `Box<int>.Get` appears only in
`ClassInterfaceTablePlan::implementationMemberOrdinals`. Stage 36 therefore knew the concrete
slot while `ProjectOopHirModel::functionForMember` had no corresponding function.

## Archivos modificados / Files modified

### `src/Frontend/ProjectMethodBodyModel.cpp`

Antes del cierre transitivo de llamadas/delegates, `build` recorre
`ProjectInterfaceModel::tablePlans()` y agrega cada implementación genérica diferida a
`demandedGenericMethods`. Una tabla de interfaz es una referencia ejecutable igual que una vtable:
su destino debe existir aunque no haya llamada sintáctica directa. El guard existente de
constraints sigue rechazando especializaciones inválidas antes de analizar el cuerpo.

Before the transitive call/delegate closure, `build` walks
`ProjectInterfaceModel::tablePlans()` and adds every deferred generic implementation to
`demandedGenericMethods`. An interface table is an executable reference like a vtable: its target
must exist even without a syntactic direct call. The existing constraint guard still rejects
invalid specializations before body analysis.

### `tests/unit/Stage44GenericsMonomorphizationTests.cpp`

`testAdvancedMonomorphization` agrega `GenericInterfaceImplementation`: `Box<T> : IValue`
implementa `Get`, y el programa sólo llega al método mediante una referencia `IValue`. La prueba
exige que el proyecto alcance OOP MIR y que la implementación cerrada aparezca en HIR.

`testAdvancedMonomorphization` adds `GenericInterfaceImplementation`: `Box<T> : IValue`
implements `Get`, and the program reaches the method only through an `IValue` reference. The test
requires the project to reach OOP MIR and the closed implementation to appear in HIR.

## Por qué esta implementación / Why this implementation

No se inventa una función en MIR ni se relaja la comprobación Stage 36. El cuerpo debe analizarse
en la misma fase que produce todos sus bindings, y el plan semántico de interfaces ya es la fuente
autoritativa de qué implementación llena cada slot. Sembrar la demanda allí conserva el modelo
lazy para métodos genéricos realmente no usados.

The fix does not fabricate a MIR function or weaken the Stage 36 check. The body must be analyzed
in the phase that produces all its bindings, and the semantic interface plan is already the
authoritative source for the implementation filling each slot. Seeding demand there preserves
lazy behavior for genuinely unused generic methods.

## Evidencia / Evidence

- antes del arreglo: `FAIL: a closed generic interface implementation is demanded by its interface table`;
- después del arreglo: Stage 44, 75/75 checks;
- regresión OOP lowering: 71/71 checks;
- compilación e instalación rápida desde disco con dos hilos;
- compilador instalado: `0.9.0+build.20260909T080755Z.54020d531848abab`;
- no se ejecutó otro baseline completo para este arreglo urgente.

## Commit de referencia / Reference commit

**Título ES:** Demanda implementaciones genéricas usadas por interfaces

**Title EN:** Demand generic implementations used by interfaces

**Descripción ES:** Trata cada slot concreto de una tabla de interfaz como demanda ejecutable,
evita que HIR omita métodos de tipos genéricos cerrados y agrega una regresión que sólo invoca la
implementación mediante la interfaz.

**Description EN:** Treats every concrete interface-table slot as executable demand, prevents HIR
from omitting closed-generic type methods, and adds a regression that invokes the implementation
only through the interface.
