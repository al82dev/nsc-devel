# Congelamiento 3.0, elemento 117 (parte 3) — operandos antes de un `loop` que se suspende

Fecha: 2026-10-26. Revisión de partida: `cc3cba07f`.

## Nota de trabajo

```text
HEAD: cc3cba07f
Elemento: 117 del plan de 142 (await en el valor de una expresión de bloque; corregir si falla)
Contrato vigente: C-02 / D-01 (la reanudación no repite operandos y todo valor vivo sobrevive a la suspensión)
Documento histórico que podría contradecirlo: 14-D01-REPETICION-AWAIT.md (el ascenso se detiene en loop)
Archivos exactos que se modificarán:
  src/Frontend/MIR/MirFunctionLowerer.cpp
  src/LLVM/Internal/OopLlvmCoroutineState.h
  src/LLVM/Internal/OopLlvmImplementation.h
  src/LLVM/Lowering/OopLlvmCoroutines.cpp
  src/LLVM/Lowering/OopLlvmControlFlow.cpp
  tests/unit/Freeze30ContractTests.cpp
```

## Problema

Después de la parte 1 (doc 40), un `loop` cuyo cuerpo se suspende compila, salvo que en la misma sentencia
algo se evalúe antes del `loop`. Sondas `A:\temp\nsc-dec\d24`:

| Sonda | Forma | Correcto | Antes |
|---|---|---|---|
| K2, K8 | `Bump() + loop { break await G(); }` / `... { int t = await G(); break t; }` | 4101 | verificador LLVM |
| K11 | `values[i] = loop { break await G(); };` | 41 | verificador LLVM |
| K13 | `Use(Bump() + 7, loop { break await G(); })` | 74101 | verificador LLVM |

```
Instruction does not dominate all uses!
  %static.call = call i32 @"...Program__Bump"()
  %add = add i32 %static.call, %loop.result
```

## Causa

La reanudación saltaba a la sentencia interior del cuerpo del `loop` (la más interna alrededor del `await`). Lo
que la sentencia de afuera evaluó antes del `loop` quedaba en un valor del poll anterior y no dominaba su uso
después del `loop`.

## Contrato

1. La sentencia que rodea al `loop` sigue siendo el destino de reanudación de los puntos de su cuerpo. Las
   sentencias del cuerpo reclaman sus ids en un ámbito propio del `loop` (`loopResumeScopes`), no en el
   despacho de la función.
2. La entrada del `loop` mira el estado de reanudación: un flujo nuevo entra al cuerpo desde arriba; la
   repetición hacia un id del cuerpo salta a la sentencia que lo reclamó. Un `loop` dentro de otro repite el
   mismo paso.
3. MIR. Para cada punto de suspensión (`await`, `yield return`, `yield;`), el recorrido de la parte 2 sigue hasta
   la raíz y, en cada `loop` que encuentra, da slot a los operandos que corren antes del `loop` en su sentencia (el
   ascenso de D-01 desde el `loop`). En la repetición se cargan y no se evalúan otra vez.
4. MoveNext de un generador síncrono recibe esos mismos slots (`buildMoveNextFrame` llama a
   `assignReplaySpillSlotNodes`). Sin un `loop` o un `match` alrededor de un `yield`, no se asigna ninguno y su
   frame no cambia.

## Validación

nsc Release y `nsc_component_tests` Debug y Release de este árbol. Sondas en `-O0` y `-O2`:

| Sonda | Forma | Ahora |
|---|---|---|
| K2, K8 | operando antes del `loop` | 4101 |
| K11 | elemento de arreglo como destino | 41 |
| K13 | argumento antes del `loop` | 74101 |
| K19 | `loop` dentro de `loop`, operando antes del exterior | 5101 |
| K20 | dos vueltas con `await` y operando antes | 8102 |
| K21 | `raii Box` en el cuerpo y operando antes | 71011 (`Box` destruido una vez) |
| K22 | `loop` en la condición de un `if` | 1101 |
| K23Gen | `yield return` en un `loop` de un generador síncrono | 14100 (`Bump` una vez) |
| K24AsyncGen | lo mismo en un generador async | 14100 |
| K5, K6, K4LoopRaii, K9, K12, K15, W2AsyncNested | partes anteriores | 41, 41, 501, 501, 4101, 41001, 151 |

K19–K24 no se midieron antes del cambio. Con la primera versión del cambio (sin el punto 4), K23Gen daba 14200:
`Bump` corría dos veces porque el MoveNext no tenía slots.

Sondas de D-01 (`d01\probe.cmd f117c -O0`): R1 106, R2 113, R3 107, R4 150, R5 306, R6 212, R7 33, R8 33, R9 207,
R11 206, R12 108, R13 107, R14 1506, R15 1506, R10Temp 108, R10After 108: sin cambios.

Regresión focal: `focal.cmd f117c` (96 y 30 casos, `awcheck` sólo cambia en `a4`), `asynccases.cmd f117c`
(86 PASS), `loopcases.cmd f117c` (10 PASS) y `gencases.cmd f117c` (los 38 casos de componente con etiqueta
`generator`, `iterator`, `yield` o `async-iterator` que no están en `asynccases`, Debug y Release: 76 PASS).

Prueba nueva: `Freeze30ContractTests::testLoopOperandResume` (4101, 8102, 71011, 14100 y 41), con las fuentes
de las sondas `d24\F117Loop*` y `K23Gen`. Compila en `A:\temp\nsc-dec\bt`; se ejecuta en el gate final.

## No verificado

- Batería completa y unitarias: gate final.
