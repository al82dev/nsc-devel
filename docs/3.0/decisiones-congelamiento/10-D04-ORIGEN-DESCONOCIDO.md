# Congelamiento 3.0 D-04 — un origen desconocido no es «se suspende»

Fecha: 2026-10-26. Rama: `development-release-candidate`. Revisión de partida: `6d941cfcc`.

## Propósito y problema

Hallazgo D-04 de la auditoría: C-02 rechazaba `await` síncronos válidos con «synchronous await of a producer that
can suspend» porque trataba todo origen desconocido como «se suspende»:

- `A1FutureParam`: `int Use(Future<int> f) { return await f; }` llamado con `Make()`, que no se suspende.
- `A2FutureReassigned`: un local `Future<int>` reasignado con otra llamada que no se suspende.

Decisiones de Alberto Huerta Luna (2026-10-26): DEC-03, un `await` síncrono cuyo productor puede suspenderse es
error de compilación; DEC-04, se conserva el pánico si un `await` clasificado como no suspendible devuelve
Pending, y los falsos positivos se corrigen en D-04.

Además, `09-AWAIT-SIN-RECURSION.md` documentó un falso negativo: con un ciclo de productores la respuesta dependía
del orden en que se revisaban los llamadores (`k2`).

## SCOPE LOCK

- **Objetivo.** Un origen desconocido se acepta; un productor conocido que se suspende sigue rechazado, sin
  depender del orden; el pánico en ejecución se conserva.
- **Naturaleza.** Corrección de contrato (C-02) por decisión explícita.
- **Debe quedar idéntico.** El diagnóstico (`SynchronousAwaitCanSuspend`, texto y posición), el lowering del
  `await` síncrono (un poll y pánico si sigue pendiente) y el `await` del punto de entrada async.
- **Modificable.** `MethodBodyCallableLifecycle.cpp`, el comentario del contrato en `ProjectMethodBodyModel.h`, el
  comentario del poll en `OopLlvmExpressionLeaves.cpp` y `Freeze30ContractTests.cpp`.
- **Prohibido.** Volver a iterar el poll (DEC-04).
- **Terminación.** A1 y A2 aceptados y ejecutados; `C02UnknownOrigin` y `k2` rechazados; el pendiente desconocido
  entra en pánico.

## Contrato

- **Orígenes de un future.** Los valores que pueden llegar al operando del `await`: un local se sustituye por el
  inicializador de cada declaración con ese nombre y el lado derecho de cada asignación a ese nombre; un
  `it.MoveNextAsync()` sobre un iterador local, por los orígenes de `it`. Cada origen que es una llamada resuelta
  nombra a un productor.
- **Desconocido.** Un parámetro, un campo, un local escrito por otra vía, una llamada sin resolver, y un productor
  que no es un callable async de fuente analizado aquí (no async, precompilado). No aporta «se suspende».
- **Se suspende.** Un productor async de fuente con `yield;`, o que alcanza uno así a través de los productores que
  nombran sus awaits. Es alcanzabilidad, así que el resultado no depende del orden ni de los ciclos.
- **Rechazo.** Un `await` síncrono se rechaza si alguno de sus orígenes conocidos nombra a un productor que se
  suspende (`C02UnknownOrigin`: `later = Quick(); later = Producer();` sigue rechazado porque `Producer` se
  suspende).
- **Ejecución.** Un origen desconocido que sigue pendiente después del único poll entra en pánico (salida 142,
  `LlvmTrapReason::Panic`).

## Cambio

- `uniqueLocalInitializer` → `appendLocalOrigins` (todas las escrituras de un local);
  `awaitOperandOrigin` → `collectAwaitOperandOrigins` (lista de orígenes, iterativa, cada nodo una vez).
- `awaitOperandCanSuspend`: verdadero solo si algún origen conocido nombra a un productor que se suspende.
- `awaitProducerCanSuspend`: alcanzabilidad con lista de trabajo; un productor no analizable no se expande;
  sin suspensión, todos los visitados quedan en el memo como `1`. Ya no existe el estado `0`.
- `Freeze30ContractTests`: `D04CycleOrder` (rechazo de `Main`), `D04FutureParam` (4), `D04FutureReassigned` (9) y
  `D04PendingPanics` (142).

## Validación

nsc Release recompilado (exit 0). Corpus de `09-AWAIT-SIN-RECURSION.md` (51 programas) más `a1` (A1), `a2` (A2),
`a3` (parámetro con un productor que se suspende) y `a4` (future en un campo), comparado con el nsc anterior:

| Caso | Antes | Ahora |
|---|---|---|
| `a1`, `a2` | rechazo C-02 | compila; salida 4 y 9 en `-O0` y `-O2` |
| `a3` | rechazo C-02 | compila; pánico, salida 142 en `-O0` y `-O2` |
| `k2` | rechaza solo `First` | rechaza `First` y `Main` |
| `k7` (iterador reasignado con `Gen()` dos veces) | rechazo C-02 | compila; salida 1 |
| `k8` (iterador inicializado consigo mismo) | `unknown identifier` + C-02 | solo `unknown identifier` |
| `a4` | rechazo C-02 | `field type has no resolved named-type layout` |
| los otros 46 | — | idénticos |

`r1..r3` (los rechazos de `testSynchronousAwaitRejected`) siguen rechazados.

## Hallazgo (no corregido aquí)

`a4`: un campo de clase de tipo `Future<int>` se rechaza con `field type has no resolved named-type layout`. Es un
rechazo explícito (no compila), pero el mensaje es interno. Antes el error de C-02 lo ocultaba. Se documenta como
limitación posterior.

## No verificado

- `Freeze30ContractTests` (unitarias) y batería completa: cierre final.
