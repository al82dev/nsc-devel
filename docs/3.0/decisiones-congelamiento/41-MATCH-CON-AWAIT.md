# Congelamiento 3.0, elemento 117 (parte 2) — `match` con un brazo que se suspende

Fecha: 2026-10-26. Revisión de partida: `b1225b9be`.

## Nota de trabajo

```text
HEAD: b1225b9be
Elemento: 117 del plan de 142 (await en el valor de una expresión de control; corregir si falla)
Contrato vigente: D-01 (la reanudación no repite los operandos de la sentencia del await)
Documento histórico que podría contradecirlo: 14-D01-REPETICION-AWAIT.md (el ascenso se detiene en match)
Archivos exactos que se modificarán:
  src/Frontend/MIR/MirFunctionLowerer.cpp
  tests/unit/Freeze30ContractTests.cpp
```

## Problema

Un brazo de `match` es una expresión. La reanudación de un `await` dentro de él repite la sentencia que contiene
el `match` (C-02). D-01 da slot a los operandos que corren antes del `await`, pero su ascenso se detiene en el
brazo y en el `match`, así que nada de lo que corre antes se guardaba. Sondas `A:\temp\nsc-dec\d24`, `G` se
suspende:

| Sonda | Forma | Correcto | Antes |
|---|---|---|---|
| K3MatchArm | `match (k) { 1 => await G(), _ => 0 }` | 41 | 41 |
| K12 | `Bump() + match (k) { 1 => await G(), _ => 0 }` | 4101 | **4201** (`Bump` dos veces) |
| K15 | `match (Key()) { 1 => await G(), _ => 0 }` | 41001 | **42001** (`Key` dos veces) |

## SCOPE LOCK

- **Cambia.** Para cada `await` que se suspende, además del ascenso de D-01, un recorrido sube hasta el primer
  límite de sentencia y, en cada `match` que encuentra, da slot a:
  - el valor que gobierna el `match` (y a cualquier otro hijo fuera del camino que sea candidato);
  - el guard del brazo elegido, si el `await` no está en el guard;
  - los operandos de la sentencia que corren antes del `match` (el mismo ascenso de D-01, desde el `match`).

  El backend ya marca y carga esos slots (`markCoroutineReplayOperands` recorre todos los ancestros).
- **Idéntico.** El backend, los ids de estado, el despacho y el ascenso de D-01. Los patterns de los brazos y los
  guards de los brazos anteriores al elegido se vuelven a probar, como antes.
- `assignReplaySpillSlotsBeside` se partió en `assignReplaySpillSlotsTo` (la lista de candidatos) y el ascenso en
  `climbReplaySpillSlots`, sin cambiar lo que hacen.

## Validación

nsc Release y `nsc_component_tests` Debug y Release de este árbol. Sondas en `-O0` y `-O2`:

| Sonda | Forma | Ahora |
|---|---|---|
| K3MatchArm | sin operandos antes | 41 |
| K12 | operando antes del `match` | 4101 |
| K15 | valor que gobierna con efecto | 41001 |
| K16 | `Bump() + match (await G()) { 4 => 7, _ => 0 }` | 7101 |
| K17 | `match (k) { 1 when Check() => await G(), _ => 0 }` | 41001 (`Check` una vez) |
| K18 | el `await` en el brazo por defecto | 4101 |

K16–K18 no se midieron antes del cambio.

Sondas de D-01 (`d01\probe.cmd f117b -O0`): R1 106, R2 113, R3 107, R4 150, R5 306, R6 212, R7 33, R8 33, R9 207,
R11 206, R12 108, R13 107, R14 1506, R15 1506, R10Temp 108, R10After 108: sin cambios.

Regresión focal: `focal.cmd f117b` (96 y 30 casos, `awcheck` sólo cambia en `a4`), `asynccases.cmd f117b`
(86 PASS) y `loopcases.cmd f117b` (10 PASS).

Prueba nueva: `Freeze30ContractTests::testMatchAwaitReplay` (4101, 401001, 410001), con las fuentes de las sondas
`d24\F117Match*`, iguales en `-O0` y `-O2`. Compila en `A:\temp\nsc-dec\bt`; se ejecuta en el gate final.

## Sigue pendiente en el elemento 117

Un operando evaluado antes de un `loop` que se suspende, en la misma sentencia (K2, K8, K11, K13): el verificador
LLVM lo rechaza.

## No verificado

- Batería completa y unitarias: gate final.
