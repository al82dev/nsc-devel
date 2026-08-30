# Barrido de EH por ejecucion real + auditoria de la suite de tests

Ronda posterior al cierre de la auditoria externa del Bloque 0. Alberto eligio explicitamente esta
opcion sobre "avanzar directo a Bloque 1", por el razonamiento que sigue.

## Por que este barrido y no Bloque 1

Las tres rondas anteriores encontraron cinco bugs en EH. Lo que tienen en comun no es el area: es
**como sobrevivieron**.

- El bug de multi-catch (segunda clausula `catch` muerta) existia porque un test asertaba
  `padLines.size() == 3` sobre el texto del IR **sin ejecutar nunca** un programa con dos
  clausulas. Fijaba la estructura rota y la daba por buena.
- B-006 (liberacion prematura) sobrevivio porque todos los tests de EH usaban un fixture **sin
  destructor**: pasaban operando sobre memoria liberada que todavia conservaba su contenido.
- Los dos "fixes" previos de B-001 (`uwtable`, la cache de cleanups) llevaban comentarios
  afirmando ser la causa raiz, sin haber corrido nada.

Tres instancias del mismo patron en la misma zona. Y esta ronda acababa de reestructurar por
completo el lowering de `catch`, cuya cobertura de tests era justamente la que habia demostrado ser
debil.

## Parte 1 -- barrido de ejecucion real

Se construyeron repros ejecutables (PE real bajo Wine, `-O0` y `-O2`) para las formas de EH que la
reestructuracion toca y que no se habian verificado: `rethrow`, `finally` dentro de `catch`,
`finally` anidado, `try/finally` sin catch, `using`, parametros `owned`, throw desde constructor,
catch dentro de un loop, y throw a traves de metodos helper (que el inliner colapsa).

**Resultado: un bug nuevo, P0, preexistente.**

### Un `finally` que simplemente no corre a -O2

```csharp
public static void Inner()
{
    try { Boom b = new Boom(); throw b; }
    finally { m = m + 1; }        // a -O2 NO corre
}
```

`-O0` da 1, `-O2` da **0**. Sin crash ni diagnostico. Afecta tambien al `Dispose()` de un `using` y
a la destruccion de un parametro `owned`.

**Causa raiz**: en la tabla de scopes, `End` es "ultima instruccion protegida + 1", asi que
cualquier bloque emitido inmediatamente despues del rango empieza en una direccion que la
personality todavia considera adentro. Cuando la ultima instruccion protegida es una llamada que
nunca retorna (un `throw`), su bloque sucesor queda vacio y el handler del `catch` se emite ahi
mismo: `TargetIp == End - 1` -> la personality concluye "no estoy abandonando este scope" y saltea
el termination handler.

**No es un defecto exclusivo nuestro**: se compilo con clang el equivalente en C SEH con una
llamada `noreturn` como unico cuerpo del try, y su binario a `-O2` bajo el mismo Wine tambien
imprime `cleanups=0`. Es un hazard de la interaccion LLVM/`__C_specific_handler`.

**Fix** (dos partes, ambas necesarias):

1. Emitir un trap real despues de toda llamada a `__nativesharp_throw`, en **las dos** ramas de
   `lowerExceptionalTransfer`. La rama sin destino de unwind importa tanto como la otra: el inliner
   la convierte en `invoke` en cualquier call site que si tenga uno, generando un `.noexc` vacio.
2. Marcar `__nativesharp_trap` como `nounwind`/`noreturn`. Es verdadero, y es lo que impide que el
   inliner lo convierta en `invoke` -- si lo hiciera, el trap **extenderia el rango protegido** y
   el handler volveria a caer en `End - 1`. Como `call` simple, sus bytes quedan fuera del rango y
   empujan al handler mas alla del final.

El primer intento (solo el trap, sin `nounwind`) fallaba exactamente por eso, medido.

Detalle completo en `BUGS_ENCONTRADOS/bug_finally_saltado_por_layout_de_handler.md`.

### Lo que el barrido confirmo sano

12 escenarios en verde en `-O0` y `-O2`, incluyendo `rethrow` corriendo su cleanup, `finally`
dentro de `catch`, throw desde constructor, `raii` sobreviviendo a tres catch en un loop, y
`using`/`owned` con destruccion exactamente-una-vez. Tres probes que fallaron al principio eran
errores mios de sintaxis (`IDisposable` no declarada, `move(...)` sobre un local no-`raii`,
`List<T>` sin referencia de proyecto), no bugs.

## Parte 2 -- auditoria de la suite de tests

Se audito la suite completa buscando asserts de la misma clase riesgosa que el que oculto el bug de
multi-catch: los que fijan la **forma** del IR sin ninguna contraparte de ejecucion.

**Hallazgo central: solo 27 de 112 archivos de test unitario ejecutan algo.** Los otros 85 asiertan
sobre el texto del IR emitido.

Los mas riesgosos, por area:

- **EH / funclets** (`Stage45LlvmCleanupFuncletTests.cpp`, `Stage45LlvmWin64EhBasicTests.cpp`,
  `Stage45TypedCatchRttiTests.cpp`): asiertan que los caminos de `using`/`finally` "contienen
  cleanuppad funclets reales" y que los cleanups "continuan el unwind con cleanupret". Ambas cosas
  eran **ciertas en el IR mientras el cleanup no se ejecutaba** -- exactamente el bug de arriba. Un
  assert de forma no puede distinguir "el funclet existe" de "el funclet corre".
- **Ownership / RAII** (`Stage50_8OwnershipIrLoweringTests.cpp`, `Stage49RaiiMoveSemanticsTests.cpp`):
  "owned-parameter cleanups reuse the existing Win64 exceptional cleanup machinery",
  "potentially throwing operations receive Win64 cleanup funclets for pending RAII owners" -- el
  perfil de riesgo exacto de B-006.
- **Conteos magicos congelados**: numeros literales de regiones de cleanup/try/catch/finally en HIR
  y MIR (`== 6`, `== 7`, `== 8`, `== 12`, `== 11`...). Dos de esos archivos ademas **se contradicen
  entre si por diseño** (`UsingRegion` 2 vs 4) y nada ejecuta para decir cual expansion es correcta.
- **Asserts de ausencia** que congelan decisiones de etapa ("Stage 45.9 does not prematurely
  implement Stage 45.10 cleanup funclets"): bloquean un fix legitimo sin probar nada de
  comportamiento.
- Un assert de **orden por posicion de bytes en el texto del dump** (`ProjectOopLoweringTests.cpp`),
  que confunde posicion textual con dominancia.

### Que se hizo con eso

No se tocaron los asserts existentes (muchos son chequeos de etapa legitimos, y reescribirlos en
masa seria una ronda propia). Lo que se hizo es **suplir la mitad que faltaba** para las
afirmaciones que importan, con una suite nueva de ejecucion real:

`tests/unit/Stage45_11EhCleanupExecutionTests.cpp` -- 12 escenarios, 120 checks, todos en `-O0` y
`-O2`, que **cuentan** cuantas veces corrio cada cleanup. Contar, y no solo "no crashea", es lo
unico que distingue una fuga (0) de una doble ejecucion (2), que son fallas con causas opuestas.

El header de ese archivo nombra explicitamente que asserts de forma de otros archivos esta
cubriendo, para que la relacion no se pierda.

## Verificacion

- Build completo con `-DNSC_WARNINGS_AS_ERRORS=ON`: limpio.
- `ctest` completo: **229/229, 100%** (228 + la suite nueva).
- Suite nueva: 120 checks; suite B-001/B-006/B-007: 130 checks.

## Recomendacion para la proxima ronda

Con esto, EH + RAII + cleanup tiene cobertura de ejecucion real razonable y ya no hay bugs
conocidos abiertos en esa area. La deuda que queda **no es de EH sino de metodo**: 85 archivos de
test que no ejecutan nada. No hace falta convertirlos todos -- lo que rinde es aplicar la regla
"todo assert que fije una decision de diseño sobre EH, cleanup, ownership o ABI necesita una
contraparte que ejecute" a medida que se toque cada area.

Bloque 1 (Ownership soundness) queda como siguiente paso segun la cadencia acordada. Vale notar que
sus items 4, 5, 8 y 10 (moved-from state, use-after-move, destruction traversal, tests negativos)
caen justo sobre el area que la auditoria marco como cubierta solo por asserts de forma
(`Stage49RaiiMoveSemanticsTests.cpp`, `Stage50_8OwnershipIrLoweringTests.cpp`), asi que conviene
encararlo con ejecucion real desde el principio y no despues.
