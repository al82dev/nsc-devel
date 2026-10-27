# Congelamiento 3.0, elemento 130 (F3) — `move` de un parámetro `T` clase en un genérico cerrado

Fecha: 2026-10-26. Revisión de partida: `b45a64498`.

## Nota de trabajo

```text
HEAD: b45a64498
Elemento: 130 del plan de 146 (defecto F3, reproducido al verificar SortedSet, elemento 123)
Contrato vigente: P3 (28-P3-BIBLIOTECA-ACCESO-POR-REFERENCIA.md: SortedSet.Add/Contains/Remove entregan su
  parámetro con move(...), «que vale para todo T»); decisión de Alberto (2026-10-26) sobre F3
Documento histórico que podría contradecirlo: ninguno
Archivos exactos que se modificarán:
  src/Frontend/Bodies/ExpressionCore/MoveExpressions.cpp
  tests/unit/Freeze30ContractTests.cpp
```

## Problema

`SortedSet<Box>` con `Box` clase no compilaba en cuanto se usaba `Contains` o `Remove`:

```text
SortedSet.ns:77:38: error: move(...) source must be a RAII local or owned parameter
SortedSet.ns:82:33: error: move(...) source must be a RAII local or owned parameter
```

P3 hizo que `Contains(T value)` y `Remove(T value)` entreguen `move(value)` a `SortedList`, para no copiar un `T`
move-only. Para un struct move-only, el parámetro por valor es dueño de su copia y se mueve. Para un `T` clase, el
parámetro sin `owned` es un préstamo, y el `move` se rechazaba antes de llegar a la regla de genéricos que ya
convierte ese `move` en copia para tipos copiables (`int`). La sonda G2 de P3 sólo cubría `int` y struct.

## Decisión de Alberto (2026-10-26)

En una especialización genérica:

- `move(p)` con `T` clase y parámetro sin `owned` copia la referencia; no transfiere ownership;
- con `T` struct move-only conserva la semántica de consumo;
- con tipos copiables como `int` queda como hoy;
- pasar ese resultado a un parámetro `owned` sigue siendo error.

Así se cumple el contrato de P3 sin cambiar la API pública de `SortedSet`/`SortedList`.

## Contrato

En `analyzeNamedParameterMove`, el rechazo «move(...) source must be a RAII local or owned parameter» no aplica
cuando el `move` es de forma copia en un tipo genérico cerrado (`genericCopyShapedMove`) y el tipo del parámetro
viene de un argumento de tipo (su `TypeSyntax` lleva el origen de la instanciación). El resultado es una copia sin
ownership, como para `int`.

## SCOPE LOCK

- **Cambia.** Sólo `move(p)` de un parámetro por valor sin `owned`, dentro de un tipo genérico cerrado, cuyo
  tipo es un argumento de tipo clase o interfaz.
- **Idéntico.**
  - Fuera de genéricos.
  - Un parámetro cuyo tipo clase escribe la plantilla (`Use(Box b)` dentro de `Wrap<T>`): sigue siendo error.
  - `T` move-only (consume) y `T` copiable (copia).
  - La entrega a un parámetro `owned`: sigue rechazada.
  - UnionWith, SymmetricExceptWith y GetViewBetween con elemento clase: siguen rechazados por C-01 (F5).

## Validación

nsc Release y `nsc_component_tests` Debug y Release de este árbol. Sondas `A:\temp\nsc-dec\d32` en `-O0` y
`-O2`:

| Sonda | Resultado |
|---|---|
| F3Class (`SortedSet<Box>`, Contains/Remove con una sonda `raii`) | 11133: encuentra y quita, 3 Box, 3 destrucciones |
| F3Struct (`SortedSet<Guard>`) | 11144, sin cambio de semántica |
| F3ToOwned (`Take(move(value))` a `owned T`) | rechazada: «owned parameter 'item' requires … transfer» |
| F3TemplateClass (`move(b)` con `Box b` escrito en la plantilla) | rechazada como antes |
| F3IntCopy (`Bag<int>`) | 700, sin cambio |

Sonda d30 SortedSetBox: 10 Box, 10 destrucciones (antes no compilaba).

Sin cambios en el resto (`seq.cmd f130f3`): `all.cmd` idéntico al de `f121` (H4SortedSetMoveOnly incluido),
`asynccases` 86, `gencases` 76 y `loopcases` 10 PASS; los 66 programas de Freeze30 en `-O0` y `-O2`;
`tests/cases`, d28 y bibliotecas como antes.

Prueba nueva: `Freeze30ContractTests::testGenericMoveReferenceCopy` (F130SortedSetClass 11133,
F130SortedSetStruct 11144, F130GenericMoveToOwned y F130TemplateClassMove rechazadas). Compila en
`A:\temp\nsc-dec\bt`; se ejecuta en el gate final.

## No verificado

- Batería completa y unitarias: gate final.
