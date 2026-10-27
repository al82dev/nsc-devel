# Congelamiento 3.0, elemento 116 — `await F(await G())` en una corrutina

Fecha: 2026-10-26. Revisión de partida: `84cbfefbc`.

## Nota de trabajo

```text
HEAD: 84cbfefbc
Elemento: 116 del plan de 142 (verificar await F(await G()); corregir si falla)
Contrato vigente: C-02 / D-01 (cada await con slot y estado propios; la reanudación no repite operandos)
Documento histórico que podría contradecirlo: 14-D01-REPETICION-AWAIT.md (observación: «sigue el modelo de C-02,
  sin cambios»; no verificado entonces)
Archivos exactos que se modificarán:
  src/LLVM/Internal/OopLlvmImplementation.h
  src/LLVM/Lowering/OopLlvmCoroutines.cpp
  tests/unit/Freeze30ContractTests.cpp
```

## Problema

Sondas `A:\temp\nsc-dec\d24`, cuerpo async (`Main` async):

| Sonda | Forma | Correcto | Antes |
|---|---|---|---|
| W1SyncNested | `await F(await G())` en `Main` síncrono | 51 | 51 |
| W2c | sólo F se suspende | 51 | 51 |
| W2a | G y F se suspenden | 51 | **11** (el await exterior da 0) |
| W2d | sólo G se suspende | 51 | **11** |
| W2AsyncNested | `Bump() + await F(await G())`, ambos se suspenden | 151 | **111** |
| W3b | `await H2(5, await G())` | 54 | **0** |
| W3c | `await H(new Box(2), await G())`, H no se suspende | 241 | **135** (referencia nula) |
| W3OwnedNested | lo mismo con H que se suspende | 241 | **135** |

Falla siempre que el await interior se suspende.

## Causa

`lowerCoroutineAwait` decide entre crear el future hijo (evaluar el operando) o reutilizar el de su slot. Crea
sólo si el estado de reanudación es 0. Al reanudar el await interior (estado = id de G), el await exterior tomaba
la rama de reutilizar: no evaluaba su operando, así que G no se reanudaba y F no se llamaba, y consultaba el slot
del exterior, que seguía en cero.

## SCOPE LOCK

- **Cambia.** El await exterior también crea su hijo cuando el estado de reanudación es el id de un await de su
  propio operando (`subtreeIds`, ya calculado por `collectCoroutineSuspensions`). Evaluar el operando llega a la
  reanudación interior; los operandos con slot de D-01 se cargan, no se evalúan otra vez.
- **Idéntico.** Ids de estado, slots, despacho, el camino de un await sin awaits anidados y el await síncrono.

## Validación

nsc Release y `nsc_component_tests` Debug y Release de este árbol.

| Sonda | Ahora (`-O0` y `-O2`) |
|---|---|
| W1, W2a, W2c, W2d, W2e (`int g = await G(); await F(g)`) | 51 |
| W2AsyncNested | 151 |
| W2b (`Bump() + await G()`, sin anidar) | 105 (antes 105) |
| W3a (`await H(new Box(2), 4)`, sin anidar) | 241 (antes 241) |
| W3b | 54 |
| W3c, W3OwnedNested | 241 (`Box` destruido una vez) |
| O4SlotCompleted (`Combine(new Box(1), await Slow())`) | 411 (antes 411) |

Sondas de D-01 (`d01\probe.cmd f116 -O0`): R1 106, R2 113, R3 107, R4 150, R5 306, R6 212, R7 33, R8 33, R9 207,
R11 206, R12 108, R13 107, R14 1506, R15 1506, R10Temp 108, R10After 108: los valores de siempre.

Regresión focal: `focal.cmd f116` (96 y 30 casos, `awcheck` sólo cambia en `a4`, el elemento 113) y
`asynccases.cmd f116`: 86 PASS.

Prueba nueva: `Freeze30ContractTests::testNestedAwaitResume` (151, 241 y 40). Sus fuentes son las sondas
`d24\F116*`, que dan esos valores en `-O0` y `-O2`. Compila en `A:\temp\nsc-dec\bt`; se ejecuta en el gate final.

## Hallazgos de la misma verificación (elementos siguientes)

- 117: un `loop` con un `await` que se suspende no compila (el verificador LLVM rechaza `loop.result.addr`).
- 118: `Combine(new Box(1), await Slow())` con `Slow` que lanza deja el `Box` sin destruir (O1, O2, O3).

## No verificado

- Batería completa y unitarias: gate final.
