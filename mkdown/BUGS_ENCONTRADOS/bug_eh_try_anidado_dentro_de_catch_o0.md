# Bug real de compilador -- un try ANIDADO dentro de un catch parecia no atrapar su excepcion

**Estado: ARREGLADO** (2026-08-29). Resulto ser un SINTOMA de
`bug_eh_raii_doble_destruccion_tras_catch_o0.md`, no un bug independiente.

Encontrado 2026-08-29 escribiendo el test de regresion obligatorio para B-001
(`tests/unit/Stage45_10EhRaiiReuseAfterCatchTests.cpp`, escenario "nested try inside catch").

## Sintoma

```csharp
raii Account account = new Account(100);
int outerCaught = 0;
int innerCaught = 0;
try { account.Withdraw(500); }              // lanza -- atrapada por el catch externo
catch (InsufficientFundsException problem)
{
    outerCaught = problem.Requested;
    try { account.Withdraw(777); }          // lanza -- DEBERIA atrapar el catch interno
    catch (InsufficientFundsException innerProblem) { innerCaught = innerProblem.Requested; }
}
account.Withdraw(10);
return (outerCaught + innerCaught + account.Balance()) % 256;   // esperado: 87
```

A `-O0` el proceso terminaba con `wine: Unhandled exception 0xe04e5301` -- el codigo de excepcion
propio de NativeSharp llegando sin manejar al sistema operativo. A `-O1`/`-O2` devolvia 87
(correcto).

## Diagnostico -- la lectura inicial era incorrecta

La lectura inicial ("el catch interno no atrapa") era **falsa**. La traza de runtime
(`WINEDEBUG=+seh`) muestra **tres** despachos de excepcion, no dos:

```
dispatch_exception code=e04e5301   -> filter llamado -> atrapada   (Withdraw(500), catch externo)
dispatch_exception code=e04e5301   -> filter llamado -> atrapada   (Withdraw(777), catch interno)
dispatch_exception code=e04e5301   -> NINGUN filter llamado -> escapa al SO
```

Los dos primeros `catch` -- incluido el interno -- funcionaban perfectamente. El tercero es
`account.Withdraw(10)`, que **no deberia lanzar nada** (balance 100, retiro 10).

Lanza porque el objeto `account` ya habia sido **destruido y liberado** durante el unwind hacia el
catch externo (bug B-006). Las asignaciones intermedias del propio manejo de excepciones reusan esa
memoria, `balance` queda con basura, la comparacion `10 > balance` da verdadero, y se lanza una
tercera excepcion que efectivamente no tiene handler.

Es decir: **el sintoma "el catch anidado no atrapa" era una consecuencia del use-after-free**, y el
`try` anidado solo lo hacia visible antes porque genera mas presion de heap que los otros
escenarios.

## Hipotesis descartadas con evidencia

Las mismas tres que en `bug_eh_raii_doble_destruccion_tras_catch_o0.md` (passes por funcion mas
amplias, `CodeGenOptLevel`, inlining), todas medidas y todas sin efecto, mas la de la cache de
cleanups indexada por ordinal (`cleanupChainCache_`), que se desactivo por completo sin que el
resultado cambiara.

## Fix

Ninguno propio: se arregla integramente con el fix de B-006 (ver ese documento). Al dejar de
destruir prematuramente el objeto, `Withdraw(10)` opera sobre un balance valido, no lanza, y el
metodo devuelve 87 en `-O0` y `-O2`.

## Verificacion

`tests/unit/Stage45_10EhRaiiReuseAfterCatchTests.cpp`, escenario 4 (`NestedTryInsideCatch`),
asertado por ejecucion real bajo Wine en `-O0` y `-O2`. Se conserva como escenario propio porque
un `try` anidado dentro de un `catch` ejercita parentesco de funclets que ningun otro escenario del
archivo cubre.

## Nota de metodo

Este caso es la razon por la que conviene desconfiar del enunciado de un sintoma antes de tener la
traza: "el catch interno no atrapa" habria llevado a investigar `catchswitch`/`catchpad` anidados
(que estaban bien) en vez de la liberacion prematura, que estaba en otro lado por completo.
