# Congelamiento 3.0, elemento 131b — `(new T(...)).Metodo()` se rechaza (C-01)

Fecha: 2026-10-26. Revisión de partida: `f11be256a`.

## Nota de trabajo

```text
HEAD: f11be256a
Elemento: 131b del plan de 148 (hallazgo T11 de la medición de D-08, doc 50)
Contrato vigente: C-01 (LostOwnership, elementos 115, 115a y 115b); decisión de Alberto (2026-10-26), abajo
Documento histórico que podría contradecirlo: ninguno
Archivos exactos que se modificarán:
  include/nativesharp/Frontend/ProjectMethodBodyModel.h
  src/Frontend/Bodies/CallAnalyzer.cpp
  tests/unit/Freeze30ContractTests.cpp
  tests/cases/projects/nsc.stage31.receivers/Receivers.ns   (autorizado, abajo)
  examples/project_stage31/App/Receivers.ns                 (autorizado, abajo)
  tests/unit/MacroAOopTests.cpp                             (autorizado, abajo)
```

## Problema

`(new Box(5)).Get()` compilaba y no destruía nunca el `Box`: ningún dueño tenía el objeto, y el método recibía un
`this` prestado que podía hacer escapar (sonda `A:\temp\nsc-dec\d36\src\T11NewReceiver.ns`: 0 destrucciones).

## Decisión de Alberto (2026-10-26)

Se rechaza con C-01/LostOwnership: el `new` no alcanzó un dueño y el método podría hacer escapar `this`. El
positivo usa primero un local `raii`/`owned` y llama el método sobre ese dueño.

Las tres pruebas históricas que usan la forma se ajustan **sólo** para expresar ownership, con el local `raii`
propuesto y los mismos valores esperados:

- `tests/cases/projects/nsc.stage31.receivers/Receivers.ns`;
- `examples/project_stage31/App/Receivers.ns`;
- `MacroAOopTests.cpp:190`.

El method value de `NativeSharp3T08MethodValueTests.cpp:263` (`(new Box()).Add`) es otro contrato y no se toca.

## Contrato

Al resolver una llamada a un método de instancia (`rejectNewCallReceiver`, junto a las demás reglas de C-01 del
llamado), si el receptor escrito es `new T(...)` de un tipo poseíble, la llamada se rechaza en el receptor:

```text
the object created by 'new' reaches no owner through the receiver of 'Get', which does not take ownership of it
and would leak; store it in a raii or owned local, pass it to an owned parameter, or return it through an owned
return type
```

## SCOPE LOCK

- **Cambia.** Sólo la llamada a un método de instancia cuyo receptor es `new` de una clase.
- **Idéntico.**
  - Los métodos estáticos.
  - La fachada de strings.
  - Los structs (`(new Vec(1, 2)).Sum()`).
  - La lectura de campo `(new Box(5)).Id`.
  - Los method values `(new Box()).Add`.

## Validación

nsc Release y `nsc_component_tests` Debug y Release de este árbol. Sondas `A:\temp\nsc-dec\d38` en `-O0` y
`-O2`:

| Sonda | Forma | Resultado |
|---|---|---|
| B1NewReceiver | `(new Box(5)).Get()` | rechazada con el diagnóstico de arriba |
| B5NewReceiverArgument | `Twice((new Box(4)).Get())` | rechazada |
| B2OwnerReceiver | `raii Box b = new Box(5); v = b.Get();` | 55: corre y destruye el objeto una vez |
| B3NewFieldRead | `(new Box(5)).Id` | 5, sin cambio |
| B4StructReceiver | `(new Vec(1, 2)).Sum()` | 3, sin cambio |

Sonda d36 T11NewReceiver: rechazada.

Resto (`seq.cmd f131b` contra `f121`):

- `all.cmd` idéntico;
- `asynccases` 86, `gencases` 76 y `loopcases` 10 PASS;
- los 66 programas de Freeze30 en `-O0` y `-O2`;
- los 27 proyectos de `tests/cases` verifican, incluido `nsc.stage31.receivers` ajustado;
- bibliotecas como antes.

Cambia una línea, por diseño: la copia de MacroA en `A:\temp\nsc-dec\d28` (fuente anterior al ajuste, que ya
fallaba por `Counter counter = new Counter(3);`) reporta además el receptor `new` de `Temporary()`.

Prueba nueva: `Freeze30ContractTests::testNewCallReceiver` (F131bNewReceiver rechazada, F131bOwnerReceiver 55).
Compila en `A:\temp\nsc-dec\bt`; se ejecuta en el gate final, igual que `MacroAOopTests` ajustada.

## No verificado

- Batería completa y unitarias: gate final. `MacroAOopTests` ya fallaba antes por otra forma de C-01
  (`Counter counter = new Counter(3);`); el ajuste no la corrige.
