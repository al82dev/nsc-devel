# Congelamiento 3.0, elemento 117 (parte 1) — `loop` cuyo cuerpo se suspende

Fecha: 2026-10-26. Revisión de partida: `485edb50e`.

## Nota de trabajo

```text
HEAD: 485edb50e
Elemento: 117 del plan de 142 (verificar await en el valor final de una expresión de bloque; corregir si falla)
Contrato vigente: C-02 / D-01 (corrutina con frame; la reanudación entra a la sentencia más interna del await)
Documento histórico que podría contradecirlo: 14-D01-REPETICION-AWAIT.md (observación: «sigue el modelo de C-02,
  sin cambios»; no verificado entonces)
Archivos exactos que se modificarán:
  src/LLVM/Lowering/OopLlvmControlFlow.cpp
  tests/unit/Freeze30ContractTests.cpp
```

## Problema

La expresión de bloque del lenguaje es `loop { ... break valor; }` (`loop-expressions.md`). Sondas
`A:\temp\nsc-dec\d24`, `Main` async y `G` que se suspende:

| Sonda | Forma | Correcto | Antes |
|---|---|---|---|
| K1SyncLoopBreak | `loop { break await G(); }` en `Main` síncrono | 41 | 41 |
| K5 | `int v = loop { break await G(); };` | 41 | verificador LLVM |
| K6 | `loop { int t = await G(); break t; }` | 41 | verificador LLVM |
| K4LoopRaii, K9 | `raii Box` en el cuerpo y `await` | 501 | verificador LLVM |
| K7 | `Bump() + loop { break 4; }` y un `await` después | 145 | 145 |

```
Instruction does not dominate all uses!
  %loop.result.addr = alloca i32, align 4
  store i32 %await.result, ptr %loop.result.addr, align 4
```

## Causa

`lowerLoopExpression` creaba el slot del resultado (`alloca`) en el bloque donde empieza el `loop`. En el poll
de una corrutina, la reanudación de un `await` del cuerpo entra directo a la sentencia interior, sin pasar por ese
punto, así que el `alloca` no domina el `break` ni la lectura del resultado.

## SCOPE LOCK

- **Cambia.** El slot del resultado se crea en el bloque de entrada de la función (`entryBlockAlloca`), como los
  demás temporales que deben dominar todo el cuerpo. No sobrevive a una suspensión ni lo necesita: el `break`
  guarda y el final del `loop` lee en el mismo poll.
- **Idéntico.** La semántica de `loop`, `break` y `continue`, los cleanups y el resultado. Fuera de una corrutina
  sólo cambia el lugar del `alloca` (además deja de crecer la pila cuando el `loop` está dentro de otro ciclo).

## Validación

nsc Release y `nsc_component_tests` Debug y Release de este árbol.

| Sonda | Ahora (`-O0` y `-O2`) |
|---|---|
| K1, K3MatchArm, K5, K6 | 41 |
| K4LoopRaii, K9 | 501 |
| K7, K10 (`3 + loop { break await G(); }`) | 145, 71 |
| K14 (`loop { break await G(); } + Bump()`) | 4101 |

Regresión focal: `focal.cmd f117a` (96 y 30 casos, `awcheck` sólo cambia en `a4`), `asynccases.cmd f117a`
(86 PASS) y `loopcases.cmd f117a` (los 5 casos de componente con etiqueta `loop`/`match`, Debug y Release:
10 PASS).

Prueba nueva: `Freeze30ContractTests::testLoopExpressionAwait` (41, 41 y 501), con las fuentes de las sondas
`d24\F117*`. Compila en `A:\temp\nsc-dec\bt`; se ejecuta en el gate final.

## Sigue pendiente en el elemento 117 (parte 2)

- Con un operando evaluado antes del `loop` en la misma sentencia, la reanudación no lo ve:
  `Bump() + loop { break await G(); }` (K2, K8), `values[i] = loop { ... }` (K11) y
  `Use(Bump() + 7, loop { ... })` (K13) siguen rechazados por el verificador LLVM.
- Un `match` con un brazo que se suspende repite lo que corre antes en su sentencia:
  `Bump() + match (k) { 1 => await G(), _ => 0 }` llama dos veces a `Bump` (K12: 4201 en vez de 4101) y
  `match (Key()) { ... }` llama dos veces a `Key` (K15: 42001 en vez de 41001).

## No verificado

- Batería completa y unitarias: gate final.
