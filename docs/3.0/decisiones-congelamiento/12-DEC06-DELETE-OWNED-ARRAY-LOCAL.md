# Congelamiento 3.0 DEC-06 — `delete[]` de un local `owned T[]` consume el ownership

Fecha: 2026-10-26. Rama: `development-release-candidate`. Revisión de partida: `30fe9b7e7`.

## Propósito y problema

P-3 (`73b03a394`) hizo que un local `owned T[]` sea dueño como `raii T[]`: se libera con sus elementos al salir del
scope. Como efecto, `delete[]` sobre él pasó a ser error («explicit delete of a RAII local is forbidden»).

- Rompió 9 proyectos históricos (hallazgo de la auditoría, sección P-3).
- Obligó a quitar 5 `delete[]` de `LinkedList.ns` y uno de cada fixture `b5_ctor_owned_field_array` y
  `c38_enum_ancho`.

Decisión de Alberto Huerta Luna (DEC-06, 2026-10-26): `delete[]` sobre un local `owned T[]` debe consumir el
ownership, marcar el local como muerto e impedir usos o destrucciones posteriores. No debe rechazarse simplemente ni
provocar doble destrucción.

## SCOPE LOCK

- **Objetivo.** `delete[] x`, con `x` local `owned T[]`, libera elementos y bloque una vez. Después `x` está movido.
  En todo camino hay exactamente una destrucción: la del `delete[]` o la limpieza léxica.
- **Naturaleza.** Corrección de contrato por decisión explícita.
- **Debe quedar idéntico.**
  - `raii T[]` y los locales `owned`/`raii` de clase siguen rechazando el borrado explícito.
  - `delete[]` de campos y elementos no cambia, ni el resto de P-3.
- **Modificable.** `ConstructionAndLifetimeExpressions.cpp`, comentarios en `ProjectMethodBodyModel.h` y
  `StatementAnalyzer.cpp`, `Freeze30ContractTests.cpp`, y los `delete[]` quitados por P-3 (ítem 86 del plan).
- **Prohibido.** Cambiar HIR, MIR o LLVM.
- **Terminación.** Sondas y fixtures de abajo.

## Cambio

`analyzeExpressionKindDeleteExpression` reconoce `delete[] x` cuando `x` cumple todo esto:

- es un local RAII;
- es un arreglo nativo;
- no está fijado (`pin`);
- está declarado con el calificador `owned` en su tipo.

En ese caso:

- no reporta el error de borrado de un local RAII;
- analiza el operando como lectura, así un `delete[]` sobre un local ya movido o posiblemente movido es «use of
  moved-from»;
- si hay un préstamo vivo del local, lo rechaza: «cannot delete[] owned local 'x' while a borrowed local is alive»;
- si no, marca la expresión en `arrayDeleteClearsOwnedField_` y el local como movido (`movedRaiiLocals_`), e
  invalida los préstamos históricos, como `move(x)`.

`arrayDeleteClearsOwnedField_` ya hacía que el HIR direccione el operando y que LLVM ponga el slot en null antes de
liberar. Es el mismo mecanismo de P-3 para campos, y no requiere cambios de backend. Con el slot en null, la limpieza
léxica del local no libera nada en el camino que borró; en los demás caminos libera normalmente. Eso funciona como
drop flag sin tocar el lowering.

Ítem 86 del plan (DEC-16): vuelven los `delete[]` que P-3 quitó.

- `LinkedList.ns`: los 5, con sus slots ya movidos antes del `delete[]`. Se conserva
  `cursor[0] = move(nextHolder[0])`, que C-12 hace indispensable: el arreglo destruye sus elementos y copiar el
  dueño lo liberaría dos veces.
- `tests/bug_triage/b5_ctor_owned_field_array/Main.ns` y `tests/cc1/c38_enum_ancho/Main.ns`: byte por byte a
  `73b03a394~1`.

`Freeze30ContractTests`: `P3OwnedArrayDelete` (rechazo) se reemplaza por siete casos:

- `DEC06OwnedArrayDelete`: 42.
- `DEC06OwnedArrayDeleteBranch`: 63.
- `DEC06OwnedArrayDeleteUnwind`: 36.
- `DEC06OwnedArrayUseAfterDelete`: rechazo.
- `DEC06OwnedArrayDeleteTwice`: rechazo.
- `DEC06OwnedArrayDeleteBorrowed`: rechazo.
- `DEC06RaiiArrayDelete`: rechazo, se conserva.

## Validación

nsc Release recompilado (exit 0). Sondas en `A:\temp\nsc-dec\d06`, en `-O0` y `-O2`:

| Sonda | Resultado |
|---|---|
| `o2` (`O2OwnedArrayDelete` de la auditoría) | 42 |
| `o3` (`delete[]` en una rama y `return`) | 56 |
| `o6` (structs con destructor, `delete[]` en una rama) | 63: cada `Guard` se destruye una vez |
| `o7` (`delete[]` en una iteración de un bucle) | 30 |
| `o11` (`delete[]` y excepción que sale del `try`) | 36 |
| `o4`, `o5` (leer o borrar después) | rechazo «use of moved-from RAII local 'values'» |
| `o10` (`ref` vivo a un elemento) | rechazo «while a borrowed local is alive» |
| `o8` (`raii int[]`) | rechazo como antes |
| `c1` (`owned Box` de clase) | rechazo como antes |

Fixtures restaurados y librería:

- `b5_ctor_owned_field_array` y `c38_enum_ancho`, compilados como proyecto y ejecutados: 42 (éxito).
- Programa con `LinkedList<Box>` (`AddFirst`, `AddLast`, `Clear`, salida del scope): 3124 en `-O0` y `-O2`. Cada
  nodo se destruye una vez y en orden.
- `--verify-project-oop-llvm` de `NativeSharp.Core.Collections.Generic`: ok.

## Observación

`o6` antes de corregirla contaba también las destrucciones de los slots en cero que hace la asignación
`g[i] = new Guard(...)`. Es D-03 (DEC-11), que se corrige aparte. Los casos de esta prueba suman `V` y por eso no
dependen de ello.

## No verificado

- `Freeze30ContractTests` (unitarias) y los otros 7 proyectos históricos de P-3: cierre final y DEC-16.
