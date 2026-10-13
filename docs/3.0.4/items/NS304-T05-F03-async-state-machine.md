# NS304-T05-F03 — Transformación productiva a state machine HIR/MIR

- **Estado**: ABIERTO (auditado; implementación pendiente)
- **Rama**: development

## Auditoría (línea base)

La vertical async NO está conectada productivamente:

- **Parser/AST** (F01, cerrado): `await` baja a `AwaitExpression`; `async` se consume sin `NS3049`.
- **Modelo de protocolo** (F02, cerrado): `FutureProtocol`/`PollOutcome`/`FutureState` en
  `Language/CoroutineSemantics.h`.
- **Plan builder**: `buildAsyncFunctionPlan` (`CoroutineSemantics.cpp:63`) reutiliza
  `SynchronousGeneratorPlan` (comparte `GeneratorLocal` para no bifurcar el ABI del frame con
  `yield`) y produce `AwaitSuspension`/`AsyncFunctionPlan`.
- **Hallazgo**: `buildAsyncFunctionPlan` NO se invoca desde `ProjectMethodBodyModel` ni
  `ProjectOopHirModel`/`ProjectOopMirModel`/`ProjectOopLlvmLowerer`. El lowering productivo de
  async (dividir por suspension points, estados de resume y terminales) no existe.

## Gaps F03

1. Sema: localizar los `AwaitExpression` de un método async, derivar `AwaitSuspension` por punto
   y construir el `AsyncFunctionPlan`.
2. HIR/MIR: transformar el cuerpo en state machine (entrada/resume/terminal) con aristas que
   conozcan locals vivos, drops y destino; no clonar el cuerpo en helpers desconectados.
3. LLVM: switch de estado + poll/resume/drop sin recursión ni allocation oculta.

## Plan

1. Cablear `buildAsyncFunctionPlan` en el análisis de cuerpos async (Sema).
2. Emitir el state machine en HIR/MIR (reutilizando la infraestructura de `SynchronousGeneratorPlan`).
3. Bajar a LLVM (switch de estados).

## Prueba puntual

Un programa async mínimo (`async int F() { await ...; return ...; }`) debe compilar, enlazar y
ejecutar con lazy start, pending→ready y cleanup.

## Nota

F03 es la primera transformación productiva desde cero de T05 (F01/F02 son modelo/AST). Requiere
vertical completa Sema→HIR→MIR→LLVM; no se introducen superficies desconectadas.

## Hallazgo adicional (2026-10-13)

El Sema **no tiene ninguna ruta para `AwaitExpression`** (`analyzeExpression` no lo contempla), y
el tipo de superficie `Future<T,E,C>` **no existe** (F02 es sólo modelo: `FutureProtocol` con claves
de tipo en string, no un tipo resoluble). Por tanto el operando de `await` no tiene tipo público que
resolver. La vertical F03 debe construir primero la superficie `Future` resoluble y después el
estado de la máquina; el plan builder (`buildAsyncFunctionPlan`) ya está unit-tested
(`TL-LANG-054..056`), pero nunca se invoca desde el pipeline productivo.

## Avance parcial (2026-10-13)

`analyzeExpressionCore` ahora reconoce `AwaitExpression` y diagnostica causalmente
`await requires the async state machine lowering (NS304-T05-F03)` (`TL-LANG-369`), reemplazando el
fallthrough silencioso que terminaba en el HIR con `unsupported Stage 34 OOP expression kind`.
Esto iguala el patrón de `yield` (T06-F03) y elimina un fallback silencioso. F03 sigue abierto:
falta la superficie `Future` + la state machine productiva.

## Auditoría de cierre (2026-10-13, segunda pasada)

Evidencia cruzada de capas (grep en `src/` e `include/`):

- **Plan builder sin consumidor productivo**: `buildAsyncFunctionPlan`, `awaitAllowed`,
  `validateFutureProtocol` y `FutureState` sólo se invocan desde tests
  (`tests/unit/NativeSharp3T10*`, `tests/compiler-suite/host/modules/LanguageCoroutinePlanningModule.cpp`).
  Ningún archivo de `src/Frontend` ni `src/LLVM` los llama.
- **Sin tipo de superficie `Future`**: no existe type-key `"Future"`/`"future:"`. Los type-keys son
  cadenas (`builtin:int`, `type:Ns::T`, `ReadOnlySpan<T>`, `Optional<T>`, …). El único `Future*` vive en
  `Language/CoroutineSemantics.h` como modelo de protocolo (F02), no como tipo resoluble.
- **Sin coroutine en HIR/MIR/LLVM**: `ProjectOopHirModel` (8.8k líneas), `ProjectOopMirModel` (3.4k) y
  `ProjectOopLlvmLowerer` (17.3k) no contienen suspend/resume/poll/estado/frame. La única
  infraestructura de state machine es el planificador semántico (`Language/CoroutineSemantics.*`,
  `Language/IterationSemantics.*`), unit-tested pero no cableado.
- **Marcador `async` sin leer en el member model**: el parser lo escribe como token `;async` en
  `auxiliaryText` (`Parser.cpp:2720` global, `Parser.cpp:4033-4035` método class/interface).
  `ProjectMemberModel.cpp:1852-1857` (declaredTypeKey) y `1980-1986` (effects) no lo consultan. Un
  método `async int F(){...}` hoy se tipa como `int F()` síncrono (semántica incorrecta: no Future lazy).
- **`await` diagnosticado causalmente**: `ProjectMethodBodyModel.cpp:9703-9711` (análogo a `yield` en
  5194-5201). Es el único punto productivo que toca `AwaitExpression`.

### Consecuencia de atomicidad

La vertical Sema→HIR→MIR→LLVM es atómica: cambiar sólo la superficie `future<T>` o sólo la resolución
de `await` en Sema, sin lowering, convierte un método `async` que hoy compila como síncrono en un
type-key `future:` no lowerable (regresión) y constituye una "superficie desconectada" (prohibida). No
existe un commit parcial seguro entre el diagnóstico causal (`4129ecc8`) y la vertical completa. F03 se
cierra con un único commit cuando `async int F(){ await …; return …; }` compile, enlace y ejecute
(lazy start, pending→ready, cleanup).

### Puntos de cableado exactos

1. `ProjectMemberModel.cpp:1852` — envolver `declaredTypeKey` en `future:<T>` cuando el marker
   `;async` está presente; `1980` — `effects.with(Effect::Async)`.
2. `ProjectMethodBodyModel.cpp:9703` — reemplazar el diagnóstico por resolución `future:<T> → T` +
   registro de `AwaitSuspension` + invocación de `buildAsyncFunctionPlan`.
3. HIR/MIR — estado de máquina (entrada/resume/terminal) reutilizando `SynchronousGeneratorPlan`
   (comparte `GeneratorLocal`).
4. LLVM — switch de estados + poll/resume/drop, sin recursión ni allocation oculta.

## Avance de implementación (2026-10-13, sin commit — vertical en construcción)

Paso 1 (superficie + member model) cableado y verificado contra `nsc.exe` reconstruido:

- `include/nativesharp/Language/CoroutineSemantics.h`: helpers inline `futureTypeKey(inner)` →
  `"future:<inner>"` y `futureInnerTypeKey(key)` → `optional<string_view>`.
- `src/Frontend/ProjectMemberModel.cpp`: incluye `CoroutineSemantics.h`; detecta el marker `async`
  (token `;async` en `auxiliaryText`), envuelve `declaredTypeKey` en `future:<T>` y añade
  `Effect::Async` a `symbol.effects`.
- Build: `ninja -C build-clang nsc` → exit 0.
- **Hallazgo de comportamiento**: el type-key `future:<T>` hoy es INERTE — el lowering no lo resuelve
  y lo ignora silenciosamente (`async int F(){ return 42; }` compila y ejecuta como `int`). Esto
  confirma que la vertical debe resolver `future:<T>` en el lowering (paso 3/4); NO es un estado
  final aceptable (sería fallback silencioso). Es el siguiente hueco a cerrar.
- **Entorno de validación**: el directorio temporal del sistema (`C:\Users\...\Temp`) devuelve
  `Access is denied` al `create_directory` del codegen particionado; la validación O0 exige fijar
  `TMP`/`TEMP` a un directorio temporal exclusivo del workspace (p.ej. `A:\nsc-devel\_tmp\codegen-tmp`),
  conforme a "directorio temporal exclusivo" del handover.

Pendiente (orden): (2b) registro de `AwaitSuspension` + `buildAsyncFunctionPlan` en el cierre del
análisis del cuerpo; (3) state machine HIR/MIR; (4) LLVM switch de estados + `future:<T>` resoluble
(struct estado/frame/resultado). Todo en un único commit cuando la prueba puntual O0 pase.

### Avance (2a) — resolución de `await` (verificado 2026-10-13)

- `src/Frontend/ProjectMethodBodyModel.cpp` (`analyzeExpressionCore`, case `AwaitExpression`):
  analiza el operando, exige `future:<T>` (`futureInnerTypeKey`) y publica `result.typeKey = T`.
  El operando no-Future se rechaza con `await operand must be a Future value (future:<T>)`.
- Verificado con `nsc.exe` reconstruido (`ninja -C build-clang nsc` exit 0): `await Other()`
  (Other → `int`) ahora diagnostica `await operand must be a Future value (future:<T>)` en vez del
  placeholder `await requires the async state machine lowering`.
- **Impacto en guards**: `TL-LANG-369` esperaba el placeholder; su fixture (`await Other()` sobre una
  función síncrona `int`) pasa a esperar el diagnóstico de no-Future. Se reescribirá/actualizará en
  el mismo commit de cierre de F03 (junto con un positivo `await <async>`). No se toca antes.

### Avance (2b) — registro de puntos `await` (verificado 2026-10-13)

- `ProjectMethodBodyModel` gana `awaitExpressions_` (orden fuente; estado = posición + 1) +
  getter `awaitExpressions()`; se limpia en `resetCallableScratch` y se registra en el case
  `AwaitExpression`. Build `ninja -C build-clang nsc` exit 0.
- Consumidores previstos: el lowering HIR/MIR (estados contiguos) y `buildAsyncFunctionPlan`.

### Avance (4-pre) — `future:<T>` resoluble en MIR (verificado 2026-10-13)

- `ProjectOopMirModel` descubre keys `future:<T>` usadas por el HIR (return/params/captures/nodos,
  igual que los tuples) y publica en `structLayouts_` el layout uniforme
  `{ i32 state @0; void* frame @8; T result @16 }` (tamaño/alineación derivados de T vía
  `valueLayoutForTypeKey`; sin heap implícito). Build `ninja -C build-clang nsc` exit 0.
- Verificado: `async int F(){ return 42; }` y `async void Pump(){}` siguen compilando/ejecutando;
  el type-key `future:<T>` ya resuelve a struct (24 bytes → sret) en vez de caer en el fallback
  silencioso anterior. **Falta**: el return-wrapping que construye el future (el cuerpo aún
  devuelve el valor interno, no el struct) y la state machine; por eso sigue sin commit.

## Diseño del transform (constructor + poll + state machine)

Detección: `ProjectOopMirFunction.effects`/`ProjectOopHirFunction.effects` ya transportan
`Effect::Async` (member model → HIR `function.effects = symbol.effects` → MIR). `EffectSet::contains`.

Para `async T F(params){ body }` el HIR/MIR genera DOS callables:

1. **Constructor `F`** (mismo nombre/mangling): `returnTypeKey = future:<T>`; cuerpo
   `return default(future:<T>)` → zero-init `{ state=0 (created), frame=null, result=default }`.
   Lazy: no ejecuta `body`.
2. **Poll `F$poll`**: firma `void F$poll(future:<T>* self, PollResult* out)`; cuerpo = `body`
   reescrito para que `return v` haga `self->state=ready; self->result=v; out->result=v`. Sin
   awaits no hay switch; con awaits, el cuerpo se parte en estados (switch sobre `self->state`).

Estado (`future:<T>.state`): 0 created, 1..N suspensión tras await i, N+1 ready. El `frame` guarda
los futures hijo pendientes (temporales que cruzan `await`) y los locals vivos (F04).

`PollResult`/`out`: discriminante Pending|Ready + slot del resultado; es un nodo nuevo
HIR/MIR/LLVM (no un sentinel textual). El punto de entrada sync→async (observar el resultado en
`Main`) requiere un bloqueo cooperativo (poll en bucle) — decisión de runtime a fijar con Alberto
antes del cierre run-pass; NO se introduce heap implícito.

Orden de implementación restante: (a) constructor + `default(future:<T>)`; (b) poll + return-wrap
(caso sin await, `completesSynchronously`); (c) split en awaits + switch (state machine); (d) frame
y PollResult; (e) entry-point. Cada paso build-verde; commit único cuando la prueba O0 pase.

### Hallazgo de mecanismo (2026-10-13): acceso a campos sintéticos del future

El return-wrap del poll escribe `self->state`/`self->result`. El nodo `FieldAddress` NO sirve para
eso: se resuelve vía `objectLayoutModel_->fieldLayoutForMember(memberOrdinal)` (ordinal de MIEMBRO
de clase), y los campos del future son slots sintéticos (offsets 0/8/16 en `structLayouts_` del MIR)
sin ordinal de miembro. Opciones: (i) nuevo nodo HIR/MIR `FutureFieldAddress` (offset directo,
índice 0=state/1=frame/2=result) resuelto contra el `ProjectOopMirStructLayout` de `future:<T>`;
(ii) modelar `Future<T>` como struct real con miembros. Elegiré (i) (offset directo) por ser lo
mínimo y no exponer campos a usuario.

## Avance de implementación (2026-10-13) — vertical (a)+(b) sin await, build/run verdes

Caso `async int F(){ return 42; }` (completesSynchronously) ya compila, enlaza y ejecuta con
`future:<T>` real (struct 24 bytes, win64-indirect) y poll generado:

- **Parser** (`parseClassMember`): el marker `async` de método de clase NO se setaba (el check era
  `FunctionDeclaration` y el nodo es `MethodDeclaration`; el token se consumía sin flag). Ahora
  `isAsync` se captura tras el loop de modificadores y se apende `;async` al `MethodDeclaration`.
  (Contradicción con el claim de F01 "marcador en class/interface method": el código lo tenía como
  "follow-up"; se cerró aquí.)
- **Member model**: `async` → `declaredTypeKey = future:<T>` + `Effect::Async` (ya verificado:
  `type=future:builtin:int effects=4`).
- **Sema** (`analyzeReturn`): desenvuelve `future:<T> → T` para el chequeo del `return` (async
  devuelve el valor interno, no el struct).
- **HIR** (`splitAsyncFunction` + `rewriteAsyncPollReturns`): genera constructor (`return
  default(future:<T>)`) + poll `F$poll(void (future:<T>* self))` que reescribe `return v` a
  `self->result = v; self->state = 1; return;` vía el nuevo nodo `FutureFieldAddress`.
- **MIR**: layout `future:<T>` en `structLayouts_`; `FutureFieldAddress` resuelve offset; se copia
  `structLayouts_` al `lowerer` (worker) para el lowering paralelo.
- **LLVM**: `llvmType("future:<T>")` → struct; `FutureFieldAddress` → GEP por offset.
- **Bugs corregidos en el camino**: (1) merge paralelo del HIR descartaba el poll (`front()` solo
  tomaba el constructor → ahora mueve todo `worker.functions_`); (2) `futureInnerTypeKey` ya no
  confunde `future:<T>*` (puntero a future) con superficie future.

Falta para el cierre run-pass de F03: (c) split en awaits + switch (state machine) para el caso
`async F(){ await …; return …; }` y (e) entry-point sync→async. El caso sin await ya es vertical
completa (constructor+poll), no una superficie desconectada.

### Avance (c-parcial) — `await` sobre future sincrónico (verificado 2026-10-13)

`async int G(){ return 5; }` + `async int F(){ int x = await G(); return x+1; }` **compila, enlaza
y ejecuta** (build verde, run exit 7 con `Main` que no llama a `F`). Lowering verificado en HIR:

- Nuevo nodo HIR/MIR `AwaitFuture`: hijo = el future (resultado del call); `calleeName` = poll
  (`G$poll`); `immediate` = offset del campo result (16). El HIR builder (`lowerExpressionImpl`,
  case `AwaitExpression`) exige un call directo a función async, baja el call y lo envuelve.
- LLVM (`AwaitFuture`): alloca el future (24 bytes), lo guarda, llama `poll(&slot)` y carga
  `slot.result` (offset 16). Patrón `TemporaryAddress` reutilizado.
- HIR verificado: `local x = await-future(static-call G()); … self->result = x + 1; self->state = 1;`.

Esto cubre `await` sobre futures que **completan sincrónicamente** (sin suspensión). Falta el
estado de máquina (suspend/resume) para `pending→ready` con future pendiente, y el entry-point
sync→async para observar el resultado en `Main`. No es un stub: el await sincrónico es un
subconjunto correcto y verificado; el caso pendiente es el paso siguiente, no un fallback.

### Decisión de diseño pendiente (2026-10-13): el frame del estado de máquina

El suspend/resume exige guardar el future hijo (y locals vivos, F04) entre suspensiones. El struct
actual `future:<T>` es UNIFORME `{ i32 state@0; void* frame@8; T result@16 }` (frame = puntero).
Opciones para el frame:

- **(A) frame inline por-función** (canónico `opaqueFutureIdentity`): el future es por-función
  (`future:<T>#hash`), con el frame inline. Correcto y sin límite, pero exige revertir el
  type-key uniforme a por-función, con el problema de orden (el hash depende del frame, que depende
  del análisis de cuerpo, posterior al member model).
- **(B) frame alocado por el caller** (uniforme, `frame*`): el caller (`await` o el entry-point)
  aloca el frame y lo pasa al constructor (cambia el ABI del constructor). Uniforme pero ABI nuevo.
- **(C) blob de frame de tamaño fijo** (uniforme, sin cambio de ABI): `frame` pasa de puntero a un
  blob inline de tamaño máximo (p.ej. 32 bytes, suficiente para 1 future hijo de 24 bytes). Es una
  LIMITACIÓN (máximo por función), no general; F04 lo refinaría a tamaño por-función.

Recomendación para cerrar el estado de máquina F03 con el mínimo riesgo y sin ABI nuevo: **(C)**,
con el límite documentado y reemplazado por (A) cuando F04 compute el frame por-función. Es una
decisión de diseño material (afecta el ABI del future, F09); se deja registrada para Alberto.

### Corrección (2026-10-13): la opción (C) tiene tamaño recursivo

Al diseñar el suspend descubrí que (C) es inviable: el blob de frame debe guardar el **future hijo**,
y el future hijo incluye su PROPIO blob de frame, de modo que el future es MÁS grande que su blob
(p.ej. blob 64 → future hijo ~80, que no cabe en 64). El tamaño "fijo" no converge. Por tanto:

- **(B) frame alocado por el caller** es la opción correcta y mínima: el future uniforme YA tiene
  `frame*` (puntero); el constructor recibe el puntero al frame (alocado por el caller con alloca,
  stack del caller = "caller-stack", sin heap implícito) y lo guarda. El frame es por-función
  (F04). Cambia el ABI del constructor (parámetro `frame*` adicional).
- **(A) inline por-función** queda como la forma canónica para F09, sin el ABI del constructor pero
  con el problema de orden del hash.

Próximo paso concreto: (B) — el constructor async recibe `void* frame` y lo guarda; el `await`
aloca el frame del hijo, lo pasa, y ante pending guarda el hijo en el frame propio + `state=N` +
suspende. State: 0=created, -1=ready (uniforme), 1..N=resume.

### Avance (encoding) — `ready = -1` (verificado 2026-10-13)

`rewriteAsyncPollReturns` ahora escribe `self->state = -1` (ready uniforme) en vez de `1`. Build
verde; verificado en HIR (`integer-constant value=18446744073709551615` = -1) y probe
(`async F(){ int x = await G(); return x+1; }` compila/ejecuta). Es la base del chequeo uniforme
`child.state == -1` del suspend, que no depende del conteo de awaits del hijo.

### Avance (state machine) — suspend/resume localizado (verificado 2026-10-13)

El nodo `AwaitFuture` ahora genera el estado de máquina completo (sin split del cuerpo):

- `select`: `self->state == 0` (entry) → `G()` fresco; si no → `self->frame` (resume, slot 0).
- `poll`: llama `G$poll(&child)`.
- `check`: `child.state == -1` (ready) → carga `child.result` (offset 16); si no → **suspend**
  (guarda el child en `self->frame`, `self->state = N`, `ret void`).

Verificado en LLVM IR: `%await.is.entry = icmp eq i32 %await.self.state, 0` → branch entry/resume;
`%await.is.ready = icmp eq i32 %await.child.state, -1` → branch ready/suspend; ready carga
`%await.result`; suspend guarda + `ret void`. Probe `await G()` (G sin await) compila/ejecuta
(ready path). El suspend path está generado y es estructuralmente correcto; aún no se EJECUTA en
runtime porque falta el frame alocado por el caller y un future pendiente (F02/F09 + entry-point).

### Guarda anti-fallback (verificado 2026-10-13)

El `AwaitFuture` anterior cargaba el `result` del hijo SIN comprobar su estado: si el productor
podía suspender (tenía `await`), se leía un resultado stale (fallback silencioso). Ahora el HIR
builder (`lowerExpressionImpl`, case `AwaitExpression`) rechaza explícitamente:
`await on an async function that can suspend requires the state machine lowering (NS304-T05-F03)`
cuando el callee contiene un `await` (recorrido `first`/`second`/`children` del body). Verificado:
`await G()` con `G` sin await → compila; con `G` que contiene `await` → rechazo explícito. Esto
elimina el fallback silencioso y delimita exactamente lo que falta: el estado de máquina.
