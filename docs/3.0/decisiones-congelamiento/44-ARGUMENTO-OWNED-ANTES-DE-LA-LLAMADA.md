# Congelamiento 3.0, elemento 118 — argumento `owned` que no llega a la llamada

Fecha: 2026-10-26. Revisión de partida: `e64631180`.

## Nota de trabajo

```text
HEAD: e64631180
Elemento: 118 del plan de 144 (operando owned en su slot con la corrutina abandonada; corregir si falla)
Contrato vigente: C-01 (todo objeto tiene dueño hasta que se destruye una vez) y C-02 / D-01 (el operando
  evaluado antes de un await vive en su slot y no se repite)
Documento histórico que podría contradecirlo: ninguno; 43-NEW-A-PARAMETRO-ORDINARIO.md fija que el `new`
  directo sólo puede ir a un parámetro owned
Archivos exactos que se modificarán:
  include/nativesharp/Frontend/ProjectOopHirModel.h
  src/Frontend/HIR/HirConversion.cpp
  src/Frontend/HIR/HirCallLowering.cpp
  src/Frontend/HIR/HirBuildCoordinator.cpp
  src/Frontend/HIR/HirExceptionAndCleanup.cpp
  src/Frontend/HIR/ExpressionStatementMachine/HirCallFrameLowering.cpp
  src/Frontend/HIR/ExpressionStatementMachine/HirConstructionFrameLowering.cpp
  src/Frontend/HIR/ExpressionStatementMachine/HirCompletionAccessFrames.cpp
  src/Frontend/HIR/ExpressionStatementMachine/DelegateInvokeFrameState.h
  src/Frontend/HIR/ExpressionStatementMachine/HirStatementFrameInitialization.cpp
  src/LLVM/Internal/OopLlvmImplementation.h
  src/LLVM/Lowering/OopLlvmCalls.cpp
  src/LLVM/Lowering/OopLlvmConstruction.cpp
  src/LLVM/Lowering/OopLlvmClosuresDelegatesEvents.cpp
  tests/unit/Freeze30ContractTests.cpp
```

## Qué significa «abandonada» aquí

Desde el lenguaje, un future suspendido no se puede soltar a medias: sólo `await` lo consulta, hasta que
termina (elemento 80). Un generador async sólo se abandona en un `yield`, que es una sentencia, así que no
queda ningún argumento pendiente. La corrutina sólo abandona un operando `owned` que está en su slot cuando,
después de reanudar, lanza antes de llegar a la llamada (sonda Q3). Es el mismo defecto que en una función
síncrona.

## Problema

Un argumento entregado a un parámetro `owned` (un `new`, un `move(x)` o un retorno owned) no tenía dueño
entre su evaluación y la llamada. Si un argumento posterior lanzaba, el objeto se perdía. Sondas
`A:\temp\nsc-dec\d26`, resultado `Made * 10 + Freed`:

| Sonda | Forma | Correcto | Antes |
|---|---|---|---|
| O1 | `Take(new Box(1), Fail())` | 11 | 10 |
| O2 | `Take(move(a), Fail())`, `a` raii | 11 | 10 |
| O3 | `Take(Make(), Fail())`, `Make` con retorno owned | 11 | 10 |
| O5, O6 | método de instancia y virtual | 11 | 10 |
| O7 | `new Holder(new Box(1), Fail())` | 11 | 10 |
| O8 | delegate `Work(owned Box b, int x)` | 11 | 10 |
| O10 | `: base(new Box(1), Program.Fail())` | 11 | 10 |
| O13 | `Take2(new Box(1), new Box(2), Fail())` | 22 | 20 |
| Q3 | `Take(new Box(1), Fail(await G()))` en async | 11 | 10 |
| Q4 | O1 dentro de un `Main` async | 11 | 10 |
| O11 | el llamado lanza | 11 | 11 |
| O12, Q2 | sin excepción (Q2 con `await` en el segundo argumento) | 11, 511 | 11, 511 |

## Contrato

1. HIR. Un argumento de tipo clase o interfaz entregado a un parámetro `owned` por valor queda en un dueño
   oculto (`OwnedTemporaryBorrow` con `booleanValue`, la marca de entrega). El dueño se activa al evaluarse
   y su cleanup corre si algo lanza antes de la llamada. Aplica en llamadas a métodos, funciones de
   namespace, llamadas ligadas, delegates, `new T(...)` y `: base(...)` / `: this(...)`.
2. LLVM. Justo antes de la instrucción de llamada (`releaseTransferredArguments`), el slot de cada argumento
   marcado se pone en cero. Desde ahí el dueño es el llamado. El cleanup de la expresión completa, o un
   unwind que sale del llamado, encuentra el slot vacío y no hace nada.
3. En una corrutina, el dueño oculto vive en el frame (D-01). Una sentencia que se reanuda después del
   `await` conserva el puntero de la primera pasada; la repetición no lo vuelve a crear.
4. Una llamada con un argumento owned queda envuelta en su expresión completa. Las dos reglas que miraban la
   llamada directamente miran ahora la de adentro: el resultado descartado que hay que destruir (C-11 / C-14,
   `Take(new Box(4));` con `Take` async) y el rechazo NS3030 de una invocación indirecta en un callable
   `noheap`.

## SCOPE LOCK

- **Cambia.** Sólo los argumentos a parámetros `owned` de tipo clase o interfaz.
- **Idéntico.** Los parámetros ordinarios y `borrowed` (con el elemento 115 no reciben un `new`); los strings
  frescos; los structs; los eventos (multicast); el receptor de una llamada.

## Validación

nsc Release y `nsc_component_tests` Debug y Release de este árbol. Sondas d26 en `-O0` y `-O2`: todos los
casos de la tabla dan el valor correcto. Sondas d25 (Y1–Y5, D1, R2, F2: 31; Y6: 5; Y7: 42) sin cambios.

Sondas de D-01 (`d01\probe.cmd f118 -O0`): R1 106, R2 113, R3 107, R4 150, R5 306, R6 212, R7 33, R8 33,
R9 207, R11 206, R12 108, R13 107, R14 1506, R15 1506, sin cambios. R10Temp y R10After pasan `new` a un
parámetro ordinario y desde el elemento 115 se rechazan; sus equivalentes con `Use(owned Box b)`
(`d26\R10TempOwned`, `R10AfterOwned`) dan 118: el `Box` se destruye una vez.

La primera versión no tenía el punto 4: `Take(new Box(4));` descartado ya no destruía el future
(`d26\W2Discard` daba 0 en vez de 1, C14Discard 0 en vez de 4). Corregido.

Programas de Freeze30 relacionados con ownership y corrutinas, compilados como proyecto y ejecutados
(`d27\f30.cmd`, `-O0` y `-O2`): los 66 dan el valor esperado (C01OwningForms, C01CatchOwnership,
C11ReturnAndAssign, C14*, F113*, F115*, F116*, F117*, F118*, P3OwnedArray*, DEC06*, D01*, C02Resume*).

Regresión focal: `all.cmd f118b` (96 y 30 casos, `awcheck` sólo cambia en `a4`), `asynccases.cmd f118b`
(86 PASS), `gencases.cmd f118b` (76 PASS) y `loopcases.cmd f118` (10 PASS).

Prueba nueva: `Freeze30ContractTests::testOwnedArgumentTransfer` (F118New, Move, Return, TwoOwned, Delegate,
Constructor, Base, AfterAwait: 11 o 22; Completes: 511). Compila en `A:\temp\nsc-dec\bt`; se ejecuta en el
gate final.

## Observado, fuera del elemento 118

`async int F() throws Boom { Fail(); return 1; }` esperado con `try { await F(); } catch (Boom) { }` en un
`Main` async termina con la excepción nativa sin atrapar (sonda `d26\Q1AsyncThrow`, exit -531737855), sin
argumentos owned de por medio. Queda registrado; no se tocó.

## No verificado

- Batería completa y unitarias: gate final.
