# Respuesta a la auditoria externa del Bloque 0 -- todos los puntos resueltos

Documento de cierre de la ronda posterior a `NativeSharp_Auditoria_Bloque0_B001_B003.md`
(auditoria externa del zip `NativeSharp_Bloque0_B001_B003_full.zip`). Alberto pidio explicitamente
resolver **todo** lo que ese documento senala. Este doc dice, punto por punto, que se verifico, que
se arreglo y como se comprobo.

Complementa `claude/30_bloque0_correctness_gate_b001_b003.md` (cierre de B-001/B-003) y corrige
algunas de sus conclusiones donde la investigacion posterior las contradijo.

---

## Resumen ejecutivo

La auditoria acerto en su veredicto de fondo: B-003 cerrado, el crash exacto de B-001 cerrado,
pero **EH + RAII no era un subsistema sano**. Lo que la investigacion mostro es que el problema era
**mas grave y mas amplio** de lo que la auditoria pudo ver:

- No era una "doble destruccion a -O0". Era una **liberacion prematura** (use-after-free) que
  ocurria tambien a **-O2**, y que **no necesitaba reuso posterior** para dispararse.
- El "try anidado dentro de catch que no atrapa" no era un bug aparte: era un **sintoma** de esa
  misma liberacion prematura.
- Y aparecio un **tercer bug P0, preexistente y no detectado por nadie**: la segunda clausula
  `catch` de un try **nunca matcheaba**.

Los tres se arreglan con un unico rediseño del lowering de `catch`. Suite completa: **228/228**,
build limpio con `-DNSC_WARNINGS_AS_ERRORS=ON`.

---

## Punto por punto de la auditoria

### B-008 -- contrato de version minima de LLVM (seccion 16-18, 43) -- ARREGLADO

La auditoria acerto. Verificado de forma directa y no por lectura: se descargo el paquete real
`llvm-19-dev` (19.1.1) y se extrajo su `llvm-c/Transforms/PassBuilder.h`, que declara **solo**
`LLVMRunPasses` a nivel de modulo. `LLVMRunPassesOnFunction` aparece recien en LLVM 20.

`CMakeLists.txt` exigia `VERSION_LESS 19` y ademas listaba `LLVM-19` entre los nombres de la
biblioteca monolitica, asi que configurar contra un LLVM 19 real pasaba el gate y fallaba despues
al compilar `LlvmOptimizer.cpp`. Corregido a **LLVM 20 minimo** en ambos lugares, con el mensaje de
error nombrando la funcion concreta que lo obliga.

### Comentarios contradictorios de causa raiz (seccion 19-22, 43) -- ARREGLADO

La auditoria acerto. Los dos comentarios afirmaban, con seguridad y sin ninguna verificacion por
ejecucion, ser la causa de B-001:

- `applyUwtableAttribute` -- reescrito. Ahora dice explicitamente que el atributo ya estaba presente
  en el binario que crasheaba y que la afirmacion anterior era **falsa**; se conserva como higiene
  correcta de Windows x64, no como fix de ningun bug conocido.
- `cleanupChainCache_` / `CachedCleanup` -- reescrito. Se agrega ademas evidencia nueva: la cache se
  **desactivo por completo** (un `cleanuppad` fresco por cadena) y se remidieron los repros de
  B-006 y B-007 con resultados byte-identicos. La cache no era la causa de nada. Esto tambien
  responde a la sospecha de la propia auditoria en sus secciones 37-38: la identidad
  `(ordinal, parentPad, next)` no es el problema.

Ambos comentarios ahora distinguen explicitamente *higiene* de *fix*, como pedia la seccion 22.

### Texto historico de `--help` (seccion 4.1, 41) -- ARREGLADO

Cortaba en Stage 48.1. Extendido hasta Stage 55 (48.2 metadata, 49 raii, 50 ownership, 51 string,
52 colecciones/`var`/`foreach`, 53 plataforma, 54 threading, 55 process/hardware/diagnostics).

### B-006 -- "doble destruccion RAII a -O0" (seccion 23-28, 43) -- ARREGLADO, y era peor

La auditoria lo clasifico como P0/P1 y acerto en la severidad, pero su caracterizacion era
incorrecta en tres puntos, todos medidos:

1. **No es especifico de -O0.** Ocurre igual a -O2. La forma del test original permitia que la
   optimizacion escondiera el sintoma.
2. **El reuso posterior no es el disparador.** Basta un local con cleanup cuyo scope encierre un
   `try` cuyo `catch` atrape.
3. **El dano real no es el conteo doble sino un use-after-free**: el objeto se destruye *y se
   libera* durante el unwind hacia el catch, con el objeto todavia en scope.

Los 4 escenarios de B-001 que estaban "en verde" usaban una clase sin destructor: **pasaban
operando sobre memoria liberada** que todavia conservaba su contenido.

**Causa raiz**, verificada en tres niveles independientes (tabla de scopes emitida, traza
`WINEDEBUG=+seh`, y un programa de referencia compilado con **clang**): `__C_specific_handler`
decide que `__finally` correr comparando rangos de IP, y el handler del `catch` siempre queda fuera
del rango del `try`, asi que un cleanup que ENCIERRA al try es indistinguible de uno que se
abandona. **El mismo construct compilado por clang muestra exactamente el mismo doble-cleanup bajo
el mismo Wine.** No es IR defectuoso de este compilador; es la semantica de la personality, y por
eso no se arregla "emitiendo mejor la tabla".

Hallazgo intermedio relevante: **el propio fix de B-001 rompia el guard existente**. El guard "ya
destruido" es un protocolo de *memoria* entre el funclet outlineado y su padre; `mem2reg` promovia
ese slot a SSA y eliminaba el `store` del funclet como definicion muerta. Es decir, el fix de B-001
extendio a -O0 un bug que a -O1/-O2 ya existia.

Detalle completo en `BUGS_ENCONTRADOS/bug_eh_raii_doble_destruccion_tras_catch_o0.md`.

### B-007 -- "try anidado dentro de catch" (seccion 29-32, 43) -- ARREGLADO, no era un bug aparte

La traza de runtime muestra **tres** despachos de excepcion, no dos: los dos `catch` (incluido el
interno) funcionaban perfectamente. El tercero era `Withdraw(10)`, que no deberia lanzar nada y
lanzaba porque el objeto ya estaba liberado y su `balance` era basura de memoria reusada.

El enunciado "el catch interno no atrapa" era enganioso y habria llevado a investigar parentesco de
`catchswitch`/`catchpad` anidados (que estaba bien). Se arregla integramente con el fix de B-006.

Detalle en `BUGS_ENCONTRADOS/bug_eh_try_anidado_dentro_de_catch_o0.md`.

### Seccion 35 de la auditoria -- "O1 arregla lo que O0 rompe" -- resuelto en el sentido correcto

La auditoria marcaba como senial arquitectonica preocupante que -O1/-O2 dieran resultados
correctos donde -O0 fallaba, y advertia que "usar O1 siempre" solo esconderia el problema. Tenia
razon en la advertencia, y la investigacion la confirmo desde el otro lado: **el bug tambien estaba
a -O2**; lo que parecia "O1 lo arregla" era la optimizacion escondiendo el sintoma en la forma
concreta del test. No quedo ninguna diferencia semantica por nivel de optimizacion: los 13
escenarios del suite se asertan en -O0 **y** -O2.

### Secciones 36-38 -- identidad del cache de cleanups -- revisado y descartado con evidencia

Revisado como pedia la auditoria. Descartado empiricamente (cache desactivada, resultados
identicos). Se conserva porque emitir un pad por scope logico es lo correcto, pero ahora esta
documentado como higiene y no como fix.

---

## Bug adicional encontrado -- P0 preexistente que nadie habia detectado

**La segunda (y siguientes) clausula `catch` de un try nunca matcheaba.**

```csharp
try { ... }                    // lanza Boom
catch (Other o) { hit = 1; }   // no matchea
catch (Boom b)  { hit = 2; }   // deberia matchear
```

Antes del fix: exit 253 en -O0 y -O2 -- la excepcion escapa sin ser atrapada. `try/catch/catch`
estaba roto en todo el lenguaje.

Causa: `__C_specific_handler` honra **exactamente un handler por catchswitch**; la numeracion de
estados SEH de LLVM emite entrada de tabla de scopes solo para el primero. Todo lo posterior era
codigo muerto.

Por que nunca se detecto: el unico test del tema asertaba

```cpp
tests.expect(padLines.size() == 3, "LLVM catchswitch owns exactly three catchpad handlers");
```

es decir, **fijaba exactamente la estructura rota y la daba por buena**, sin ejecutar jamas un
programa con dos clausulas. Es el mismo modo de falla que la regla "real-execution-first" del
proyecto existe para evitar. Ese assert fue corregido para fijar la estructura correcta, con una
nota explicando por que la anterior estaba al reves.

Detalle en `BUGS_ENCONTRADOS/bug_segunda_clausula_catch_nunca_matchea.md`.

---

## El fix (uno solo para los tres bugs)

`lowerTryRegion` en `src/LLVM/ProjectOopLlvmLowerer.cpp`:

- El `catchswitch` unwindea **to caller**, lo que hace desaparecer la entrada `__finally` sobre el
  rango del try (verificado sobre la tabla de scopes emitida) y con ella la destruccion prematura.
- Posee **un unico `catchpad`**, filtrado por `__nativesharp_eh_filter` (filtro de dominio), cuyo
  cuerpo solo hace `catchret` a codigo normal del padre.
- Ese codigo del padre re-testea el objeto atrapado contra el id de tipo RTTI estable de cada
  clausula **en orden de fuente** y salta al cuerpo correspondiente -- por lo que todas las
  clausulas funcionan, no solo la primera.
- Si ninguna matchea, corre los cleanups que encierran al try de forma **explicita** (por
  construccion, solo se llega ahi cuando la excepcion realmente se va) y **re-lanza el mismo objeto
  de excepcion**, para que los filtros tipados de frames externos vean el tipo que habrian visto.

Mas, como soporte:

- **Guard de cleanup con slot propio** (`ensureCleanupGuard` / `emitCleanupGuardBranch`): el estado
  "ya destruido" vive en un `alloca i8` dedicado con accesos `volatile`. Se probo primero marcar
  `volatile` el slot del puntero owner; eso **reintrodujo el crash de B-001 a -O0**, medido. Con un
  flag separado, el puntero sigue siendo promocionable (B-001 sigue arreglado) y solo el bit cruza
  la frontera funclet -> padre.
- Bindings nuevos en `LlvmCApi.h`: `LLVMSetVolatile`, `LLVMGetEntryBasicBlock`,
  `LLVMGetFirstInstruction`, `LLVMPositionBuilderBefore`.

Efecto colateral deliberado: como los cuerpos de las clausulas pasan a ser codigo normal del padre,
una excepcion lanzada dentro de un `catch` se baja con la maquinaria normal, sin las restricciones
de parentesco de funclets que antes forzaban compromisos.

### Limitacion conocida, declarada

Una excepcion SEH **ajena** (por ejemplo un access violation del sistema, no generada por `throw`)
es declinada por el filtro de dominio y ya no ejecuta los cleanups de este frame al propagarse.
Antes si los ejecutaba. En la practica ninguna de esas excepciones era manejable por el lenguaje
igualmente (terminaban el proceso), pero queda anotado como diferencia real de comportamiento y no
escondido.

---

## Verificacion

- Build completo con `-DNSC_WARNINGS_AS_ERRORS=ON`: limpio.
- `ctest` completo: **228/228, 100%**.
- `tests/unit/Stage45_10EhRaiiReuseAfterCatchTests.cpp`: **130 checks**, 13 escenarios, todos por
  ejecucion real bajo Wine y **todos asertados en -O0 y -O2**. Ya no queda ningun "known issue"
  informativo: los dos que el zip anterior reportaba sin fallar CTest ahora fallan si regresan.
- Dos asserts de forma de IR de tests existentes (`nsc.stage45.typed-catch-rtti`,
  `nsc.stage45.llvm-win64-eh-basic`) fueron actualizados porque fijaban la estructura anterior; en
  ambos casos con un comentario explicando que cambio y por que, y moviendo la garantia de
  comportamiento a una verificacion por ejecucion real.

Escenarios nuevos (7-13), que existen porque los seis originales no podian detectar nada de esto
(todos usaban un fixture sin destructor y una sola clausula `catch`):

7. el objeto sigue vivo e intacto tras el catch **forzando 40 asignaciones intermedias** -- sin ese
   churn de heap, la memoria liberada conserva su contenido y un use-after-free "pasa" el test;
8. destruccion exactamente-una-vez **sin reuso posterior**;
9. un `raii` declarado DENTRO del try SI se destruye al lanzar (guarda contra sobre-corregir y
   convertir el double-free en un leak);
10. una excepcion que ninguna clausula atrapa SI corre el cleanup que encierra al try;
11. la clausula que matchea es la SEGUNDA;
12. `catch` sin tipo despues de una clausula tipada, con `raii` encerrando el try;
13. `finally` corre tanto cuando el catch matchea como cuando no.

---

## Estado

Todos los puntos de la auditoria: **resueltos**. Los dos bugs que ella dejaba abiertos (B-006,
B-007) mas el bug de contrato (B-008), mas los dos items documentales, mas un tercer bug P0 que
encontro esta ronda y que la auditoria no vio.

EH + RAII ya puede declararse sano **para lo que hay cobertura de ejecucion real**: destruccion
exactamente-una-vez, sin liberacion prematura, con seleccion de clausulas correcta y `finally`
correcto en ambos caminos, en -O0 y -O2. Queda como candidato futuro (no bloqueante, y no reportado
por la auditoria) evaluar si conviene migrar a `__CxxFrameHandler3`, que modela cleanups por
numeros de estado en vez de rangos de IP y haria innecesario el guard.
