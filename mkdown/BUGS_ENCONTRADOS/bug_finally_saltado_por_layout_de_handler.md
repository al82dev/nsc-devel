# Bug real -- un `finally` (o cleanup de `using`/`owned`) SALTEADO silenciosamente a -O2

**Estado: ARREGLADO** (2026-08-29). Bug **preexistente**, no reportado por ninguna auditoria.
Es un hazard a nivel LLVM/SEH, **no un defecto exclusivo de NativeSharp** -- clang lo reproduce
identico. Ver "Verificacion contra clang".

Encontrado 2026-08-29 en el barrido de ejecucion real de EH posterior al cierre de la auditoria del
Bloque 0.

## Sintoma

```csharp
class Program
{
    private static int m;
    public static void Inner()
    {
        try { Boom b = new Boom(); throw b; }
        finally { m = m + 1; }        // <-- NO corre a -O2
    }
    public static int Main()
    {
        try { Inner(); } catch (Boom b) { }
        return m;                      // esperado 1, a -O2 devuelve 0
    }
}
```

`-O0`: 1 (correcto). `-O2`: **0** -- el `finally` simplemente no se ejecuta. Sin crash, sin
diagnostico, sin ninguna senial. Para un `finally` que libera un handle, suelta un lock o cierra un
archivo, esto es una fuga silenciosa.

Confirmado **preexistente**: se compilo el mismo repro con el binario del zip anterior
(`NativeSharp_Bloque0_B001_B003_full.zip`) y da 1 / 0 igual.

Afecta a las tres formas de cleanup, no solo a `finally`: tambien al `Dispose()` de un `using` y a
la destruccion de un parametro `owned`, medidos por separado.

## Causa raiz

`__C_specific_handler` decide si un scope de terminacion esta siendo abandonado comparando la IP
destino del unwind contra el rango del scope:

```c
if (ControlPc dentro de [Begin,End)) {
    if (JumpTarget) continue;
    if (TARGET_UNWIND && TargetIp dentro de [Begin,End)) break;   // el destino sigue adentro
    handler(TRUE, frame);
}
```

En la tabla de scopes que emite LLVM, `End` es **"ultima instruccion protegida + 1"** (la
convencion existe para que la direccion de retorno de un `call` cuente como dentro del rango). Por
lo tanto **cualquier bloque emitido inmediatamente despues del rango empieza en una direccion que
la personality todavia considera adentro**.

Cuando la ultima instruccion protegida es una llamada que nunca retorna -- un `throw` -- su bloque
sucesor normal queda vacio (`unreachable` no emite bytes), y el handler del `catch` se emite
justo ahi. Resultado: `TargetIp == End - 1`, la personality concluye "el destino sigue dentro de
este scope, no lo estoy abandonando", y **saltea el termination handler**.

Verificado sobre el ensamblador emitido:

```
.Ltmp8:  callq __nativesharp_throw
.Ltmp9:                                  <- End = .Ltmp9 + 1
# %bb.4:  (vacio)
.LBB1_12:  # eh.catchpad.entry           <- TargetIp == .Ltmp9  ==> ADENTRO del rango
```

A `-O0` habia instrucciones sueltas en el medio que corrian el handler fuera del rango y tapaban el
problema; a `-O2` no.

## Verificacion contra clang -- esto NO es un defecto exclusivo nuestro

Se compilo con **clang** el equivalente en C SEH con una llamada `noreturn` como unico cuerpo del
try:

```c
__declspec(noreturn) static void thrower(void) { RaiseException(0xE0001234,0,0,0); for(;;){} }
void f(void) {
  __try {
    __try { thrower(); }
    __finally { cleanup(); }
  } __except(EXCEPTION_EXECUTE_HANDLER) { handler(); }
}
```

Salida de clang a `-O2` bajo el mismo Wine:

```
EXCEPT ran, cleanups=0
FINAL cleanups=0
```

**El binario de clang saltea su propio `__finally` exactamente igual.** El hazard es de la
interaccion LLVM/`__C_specific_handler`, no de este compilador. (Con codigo de camino normal entre
el rango y el handler -- el caso habitual en C -- clang no lo dispara, y por eso casi nunca se ve.)

## Por que agregar instrucciones "adentro" NO alcanza

El primer intento fue emitir un trap despues del `throw`. No alcanza: al inlinear, LLVM convierte
ese trap en un `invoke` (porque esta dentro de un `try` y podria unwindear), lo que **extiende el
rango protegido** hasta pasar el trap; el `.noexc` del trap queda vacio y el handler vuelve a caer
justo en `End - 1`. Encadenar traps solo corre el problema un nivel.

La clave es que la instruccion que separa tiene que quedar **FUERA** del rango.

## Fix

Dos cambios, ambos necesarios:

1. **Emitir un trap real despues de TODA llamada a `__nativesharp_throw`**, en las dos ramas de
   `lowerExceptionalTransfer` (con y sin destino de unwind). La rama sin destino importa tanto como
   la otra: es la que se usa en un metodo cuyo unico statement es `throw`, y el **inliner** la
   convierte en `invoke` en cualquier call site que si tenga uno, sintetizando un `.noexc` vacio.
   Ademas es correcto por si mismo: si `__nativesharp_throw` alguna vez retornara, se falla
   ruidosamente en vez de caer en el handler (`LlvmTrapReason::ThrowReturned`).

2. **Marcar `__nativesharp_trap` como `nounwind` (y `noreturn`)**. Es verdadero -- el trap termina
   el proceso, nunca lanza -- y es lo que impide que el inliner lo convierta en `invoke`. Al quedar
   como `call` simple, sus bytes caen **fuera** del rango protegido, y son precisamente esos bytes
   los que empujan al handler mas alla de `End`.

## Verificacion

`tests/unit/Stage45_11EhCleanupExecutionTests.cpp` -- suite nueva, 12 escenarios, **por ejecucion
real bajo Wine y en -O0 y -O2**, que **cuentan** cuantas veces corrio cada cleanup (0 = fuga,
2 = doble ejecucion; ambas fallas tienen causas opuestas y "no crashea" no distingue ninguna):

- `finally` con excepcion que escapa, inline y via helper inlineado;
- dos `finally` anidados, ambos deben correr;
- `finally` interno + catch externo + `finally` externo (1/10/100, los tres observables por separado);
- `finally` dentro del cuerpo de un `catch`;
- `using` con throw, y `using` cuyo scope encierra un try que atrapa;
- parametro `owned` con throw, y `owned` cuyo metodo atrapa;
- `rethrow` corriendo el cleanup que lo encierra;
- throw desde un constructor con un `raii` vivo;
- `raii` sobreviviendo a tres catch consecutivos dentro de un loop.

## Nota de metodo -- por que esto no se habia visto

Una auditoria de la propia suite de tests (2026-08-29) encontro que **solo 27 de 112 archivos de
test unitario ejecutan algo**; el resto asierta sobre el TEXTO del IR emitido. Concretamente,
`Stage45LlvmCleanupFuncletTests.cpp` asierta que los caminos de `using`/`finally` "contienen
cleanuppad funclets reales" y que los cleanups "continuan el unwind con cleanupret" -- ambas cosas
eran ciertas en el IR **mientras el cleanup no se ejecutaba**. Un assert sobre forma de IR no puede
distinguir "el funclet existe" de "el funclet corre".
