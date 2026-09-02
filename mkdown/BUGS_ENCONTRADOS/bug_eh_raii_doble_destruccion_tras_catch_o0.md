# Bug real de compilador -- destruccion PREMATURA (y por tanto use-after-free) de un objeto RAII
# cuyo scope ENCIERRA un try cuyo catch atrapa

**Estado: ARREGLADO** (2026-08-29). Ver seccion "Fix" al final.

Encontrado 2026-08-29 escribiendo el test de regresion obligatorio para B-001
(`tests/unit/Stage45_10EhRaiiReuseAfterCatchTests.cpp`). Distinto de B-001 y NO resuelto por el fix
de B-001 (`mem2reg` por funcion en `LlvmOptimizer.cpp`).

## Sintoma inicial (enganioso) y sintoma real

El sintoma con el que aparecio fue "el destructor corre dos veces a -O0". Ese enunciado resulto
ser **doblemente incorrecto**, y esto importa porque marco mal la severidad y el alcance:

1. **No es especifico de -O0.** Ocurre igual a -O2; lo que cambia es que la forma concreta del test
   original permitia que la optimizacion escondiera el sintoma. Medido con una matriz de 5 formas
   estructurales x 2 niveles de optimizacion.
2. **El reuso posterior no es el disparador.** Basta con tener un local con cleanup cuyo scope
   encierre un `try` cuyo `catch` efectivamente atrape. No hace falta volver a usar el objeto.
3. **Lo grave no es el conteo doble, es que el objeto se LIBERA mientras sigue vivo.** El
   destructor corre y `__nativesharp_free` libera la memoria durante el unwind hacia el `catch`,
   con el objeto todavia en scope. Todo uso posterior es un **use-after-free**.

El conteo doble era simplemente la unica consecuencia que un test con destructor observable podia
ver. Los 4 escenarios de B-001 que estaban "en verde" usaban una clase SIN destructor y por lo
tanto **pasaban operando sobre memoria liberada** que casualmente todavia conservaba su contenido.

## Matriz estructural que aisla el disparador (ejecucion real bajo Wine)

| Forma | -O0 | -O2 |
|---|---|---|
| raii encierra try; throw atrapado; reuso posterior | 2 | 2 |
| raii encierra try; throw atrapado; SIN reuso | 2 | 2 |
| raii encierra try; NO se lanza nada | 1 | 1 |
| raii declarado DENTRO del try | 1 | 1 |
| raii en otro metodo (el throw cruza el frame) | 1 | 1 |

Disparador exacto: **local con cleanup cuyo scope encierra un try cuyo catch atrapa, en el mismo
metodo**.

## Causa raiz (verificada, no inferida)

`__C_specific_handler` -- la personality de Windows x64 que este compilador usa -- decide que
manejadores de terminacion (`__finally`) correr durante un unwind **comparando rangos de IP**:

```c
if (ControlPc dentro de [Begin,End)) {
    if (JumpTarget) continue;                 // entradas de __except: se saltean en unwind
    if (TARGET_UNWIND && TargetIp dentro de [Begin,End)) break;   // el destino sigue dentro: no salir
    handler(TRUE, frame);                     // si no: se considera que se sale de este scope
}
```

LLVM baja el borde `unwind` de un `catchswitch` a una entrada `__finally` cuyo rango cubre el
**rango de instrucciones del TRY**. El handler del `catch` siempre queda emitido FUERA de ese
rango, asi que `TargetIp` nunca cae dentro y la personality concluye que el cleanup que ENCIERRA
al try se esta abandonando -- y lo ejecuta.

Verificado en tres niveles independientes:

1. **Tabla de scopes emitida** (`llc` + `.seh_handlerdata`): dos entradas con el MISMO rango, la
   del `__except` y la del `__finally`.
2. **Traza de runtime** (`WINEDEBUG=+seh`): `__C_specific_handler calling __finally ...` durante
   el unwind hacia el catch.
3. **Programa de referencia de clang**: se compilo el construct C equivalente
   (`__try { __try {raise} __except {...} } __finally {cleanup}`) con **clang** para Windows y se
   corrio bajo el MISMO Wine. Salida de clang:
   ```
   CLEANUP ran (count=1)      <- durante el unwind al __except
   HANDLER: count so far=1
   CLEANUP ran (count=2)      <- camino normal
   FINAL count=2
   ```
   Es decir: **el propio clang produce exactamente el mismo doble-cleanup**. Esto NO es IR
   defectuoso de NativeSharp; es la semantica documentada de rangos de IP de esta personality, y
   por lo tanto no se arregla "emitiendo mejor la tabla de scopes".

## Hipotesis descartadas con evidencia (ninguna adivinada)

1. **La cache de cleanup (`cleanupChainCache_`, indexada por ordinal)** -- descartada. Se desactivo
   por completo (un `cleanuppad` fresco por cadena) y se remidieron ambos repros: resultados
   byte-identicos. La auditoria externa la sugeria como sospechosa (su seccion 37/38); no lo es.
2. **Passes de limpieza adicionales por funcion** -- descartada: `mem2reg,simplifycfg` y
   `mem2reg,sroa,early-cse,instcombine,simplifycfg` no cambian nada.
3. **El `CodeGenOptLevel` del backend** -- descartada: forzar `LLVMCodeGenLevelLess` (el de -O1)
   manteniendo el pipeline de IR no cambia nada.
4. **Falta de inlining** -- descartada: `cgscc(inline)` antes de `function(mem2reg)` no cambia nada.

## Un hallazgo intermedio que importa: el fix de B-001 rompia el guard existente

El compilador ya tenia un guard "ya destruido" (poner en null el slot del owner antes de destruir).
Ese guard es un **protocolo de memoria entre el funclet de cleanup y su funcion padre**. Un funclet
se outlinea en codegen, asi que un valor definido dentro de el no es observable en el padre: el
protocolo solo funciona mientras el slot siga en memoria.

`mem2reg` -- que el fix de B-001 corre sobre toda funcion con personality SEH -- promueve ese slot a
SSA, con lo cual el `store` de "marcar destruido" del funclet queda como una definicion muerta
dentro del funclet y se elimina. Verificado comparando el IR y el asm con y sin `mem2reg`:

```
sin mem2reg (funclet):  movq $0, -8(%rbp)      <- el guard
con mem2reg (funclet):  <la instruccion desaparecio>
```

O sea: **el fix de B-001 extendio a -O0 un bug que a -O1/-O2 ya existia** (esos niveles siempre
corren `mem2reg`).

## Fix aplicado

Dos cambios, ambos necesarios:

1. **Guard con slot propio** (`ensureCleanupGuard` / `emitCleanupGuardBranch` en
   `ProjectOopLlvmLowerer.cpp`): el estado "ya destruido" pasa a un `alloca i8` dedicado, con
   accesos `volatile`, asignado en el bloque de entrada. Se probo primero marcar `volatile` el
   propio slot del puntero owner; eso **reintrodujo el crash de B-001 a -O0**, medido. Separar el
   flag lo evita: el puntero solo fluye padre -> funclet (direccion que LLVM ya resuelve por
   spilling) y solo el bit destruido/no-destruido fluye funclet -> padre. Asi el puntero sigue
   siendo promocionable (B-001 sigue arreglado) y el flag queda en memoria (B-006 arreglado).

2. **La destruccion prematura se elimina de raiz** (`lowerTryRegion`): el `catchswitch` ya no usa
   la cadena de cleanups que ENCIERRA al try como destino de `unwind`; unwindea **to caller**, lo
   que hace desaparecer por completo esa entrada `__finally` sobre el rango del try (verificado
   sobre la tabla de scopes emitida). Los cleanups que encierran al try se ejecutan de forma
   explicita en el camino "ninguna clausula matcheo", que por construccion solo se alcanza cuando
   la excepcion realmente se va del try.

## Verificacion

`tests/unit/Stage45_10EhRaiiReuseAfterCatchTests.cpp`, escenarios 6-10 y 13, todos por ejecucion
real bajo Wine y en -O0 y -O2:

- destruccion exactamente-una-vez con y sin reuso posterior;
- **el objeto sigue vivo e intacto tras el catch aun forzando 40 asignaciones intermedias** (esta
  es la comprobacion cuya ausencia permitio que el bug pasara desapercibido: sin churn de heap, la
  memoria liberada conserva su contenido y un use-after-free "pasa" el test);
- un raii declarado DENTRO del try SI se destruye al lanzar (guarda contra sobre-corregir el fix y
  convertir el double-free en un leak);
- una excepcion no atrapada por ninguna clausula SI corre el cleanup que encierra al try;
- `finally` corre tanto en el camino con catch que matchea como en el que no.
