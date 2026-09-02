# Bug real de compilador -- la SEGUNDA (y siguientes) clausula `catch` de un try nunca matcheaba

**Estado: ARREGLADO** (2026-08-29). Bug **P0 preexistente**, no detectado por ninguna auditoria ni
por ningun test del proyecto.

Encontrado 2026-08-29 mientras se arreglaban B-006/B-007, al investigar por que una clausula
sintetica agregada a un `catchswitch` no aparecia en la tabla de scopes emitida.

## Sintoma

```csharp
Acc account = new Acc(100);
int hit = 0;
try { account.Withdraw(500); }         // lanza Boom
catch (Other o) { hit = 1; }           // NO matchea
catch (Boom b)  { hit = 2; }           // SI deberia matchear
return hit;                            // esperado 2
```

Ejecucion real bajo Wine, antes del fix: **exit 253 en `-O0` y en `-O2`** -- la excepcion escapa
sin ser atrapada por ninguna clausula. Nunca devolvia 2.

Es decir: **`try { } catch (A) { } catch (B) { }` estaba roto para cualquier clausula que no fuera
la primera**, en todo el lenguaje, en todos los niveles de optimizacion.

## Causa raiz

`__C_specific_handler` modela un `__try/__except` con **exactamente un filtro**. Cuando LLVM baja un
`catchswitch` con varios `catchpad` para esta personality, su numeracion de estados SEH emite una
entrada de tabla de scopes **solo para el primer handler**.

Verificado directamente sobre el ensamblador emitido: un `catchswitch` con dos handlers produce una
unica entrada de filtro en `.seh_handlerdata`. La segunda clausula quedaba como codigo muerto: el
`catchpad` existia en el IR y su cuerpo se emitia, pero **nada en la tabla de scopes lo podia
alcanzar jamas**.

## Por que nunca se detecto

El unico test que tocaba el tema (`nsc.stage45.typed-catch-rtti`) asertaba la forma del IR:

```cpp
tests.expect(padLines.size() == 3, "LLVM catchswitch owns exactly three catchpad handlers");
```

Es decir, **fijaba exactamente la estructura rota** y la daba por buena, sin ejecutar nunca un
programa con dos clausulas para ver cual de ellas realmente atrapaba. Todos los demas tests de EH
del proyecto usan una sola clausula `catch`, asi que el bug era invisible para la suite completa.

Este es el mismo modo de falla que el proyecto ya se habia impuesto evitar con la regla de
"real-execution-first": un assert sobre texto de IR puede confirmar felizmente una estructura que
no funciona.

## Fix

`lowerTryRegion` en `src/LLVM/ProjectOopLlvmLowerer.cpp` deja de emitir un `catchpad` por clausula.
Ahora emite **uno solo**, filtrado por `__nativesharp_eh_filter` (el filtro de dominio: "esta
excepcion es de las nuestras"), cuyo cuerpo no hace mas que `catchret` a codigo normal del padre.
Ese codigo del padre vuelve a testear el objeto atrapado contra el id de tipo RTTI estable de cada
clausula **en orden de fuente** (`__nativesharp_rtti_is_class`) y salta al cuerpo correspondiente;
si ninguna matchea, corre los cleanups que encierran al try y re-lanza.

Efecto colateral deliberado y valioso: como los cuerpos de las clausulas pasan a ser codigo normal
del padre en vez de interiores de funclet, una excepcion lanzada dentro de un `catch` se baja con la
maquinaria normal y unwindea por los cleanups que la encierran como cualquier otro codigo del scope,
sin las restricciones de parentesco de funclets que antes obligaban a compromisos.

Este mismo cambio es el que arregla B-006 y B-007 (ver esos documentos): son un unico rediseño.

## Verificacion

`tests/unit/Stage45_10EhRaiiReuseAfterCatchTests.cpp`, por ejecucion real bajo Wine en `-O0` y
`-O2`:

- escenario 11 (`SecondCatchClauseMatches`): la clausula que matchea es la SEGUNDA -> devuelve 2;
- escenario 12 (`CatchAllAfterTypedClause`): un `catch` sin tipo despues de una clausula tipada
  atrapa lo que aquella declina, y el objeto `raii` que encierra al try sigue vivo despues.

Ademas se corrigio el assert de forma de IR de `Stage45TypedCatchRttiTests.cpp` para que fije la
estructura correcta (un catchpad + dispatch por tipo en orden de fuente) en vez de la rota, con una
nota explicando por que la asercion anterior era exactamente al reves.
