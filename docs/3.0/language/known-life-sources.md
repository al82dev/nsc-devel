# Fuentes de vida conocida / Known-life sources

## ES — Contrato normativo

### Garantía

NativeSharp impide fugas, doble destrucción y escapes cuando la fuente y su vida son conocidas estáticamente. Las
referencias no owned provenientes de parámetros o campos permanecen bajo responsabilidad del programador.

La garantía cubre exactamente las fuentes y los sumideros de abajo. Lo que no está en esta lista no está
garantizado; los límites se enumeran al final.

### Fuentes de vida conocida

| Fuente | Su vida termina |
|---|---|
| Temporal owned: resultado de una llamada con retorno `owned`, o `new` de clase | al final de la expresión completa |
| Local `raii` | al salir de su scope |
| Objeto `scope` | al salir de su scope; vive en el frame |
| Excepción atrapada (variable del `catch` y sus alias del mismo handler) | al terminar el handler |
| Préstamo de excepción (parámetro no `owned` de tipo excepción y sus alias en la llamada) | al volver de la llamada |
| Local `pin` | no cruza una suspensión |

Un solo clasificador del compilador (`KnownLifeSources`) reconoce estas fuentes. Cada regla de abajo le pregunta
a él y conserva su propio diagnóstico.

### Reglas

1. **Temporal owned.**
   - Se destruye exactamente una vez, al final de la expresión completa, después de todo otro uso y en orden
     inverso de creación.
   - Si un `?:` o `??` no lo creó en el camino tomado, no se destruye.
   - Un préstamo del temporal no puede inicializar un local, asignarse, devolverse ni entregarse con `yield`.
     Es préstamo:
     - una lectura de campo de referencia o string sobre el temporal o sobre otro préstamo;
     - un getter así;
     - el resultado referencia o string, no `owned`, de una llamada (método, función o delegate) cuyo
       receptor o algún argumento es un préstamo, o cuyo argumento es el propio temporal owned entregado a
       un parámetro que no toma ownership (`Id(MakeText())`);
     - un `?:` o `??` con alguna rama que lo sea.
   - Leer un valor copiable del temporal no es un préstamo.
   - Si el préstamo termina en un valor simple, en un owner nuevo (un string nuevo, el objeto que devuelve un
     método llamado sobre el préstamo) o en una escritura de campo, el temporal vive hasta el final de la
     expresión.
   - Un temporal prestado como argumento también vive hasta el final de la expresión completa, así que
     `Id(MakeText()).Length` lo lee vivo.
   - `(new T(...)).Metodo()` es error: el objeto no tiene dueño.
2. **Local `raii`.** No se copia a un local no dueño, por asignación ni en una captura sin `move`. No se devuelve
   sin `return move(...)`.
3. **Objeto `scope`.**
   - No se guarda en un campo ni en un static. Basta que el lado derecho nombre el objeto.
   - No se devuelve ni se captura.
   - Devolver un valor leído de él (`return b.Id;`) es válido.
4. **Excepción atrapada.**
   - No se devuelve, no se guarda donde sobreviva al handler y no se captura.
   - Un local del mismo handler puede guardarla y queda bajo la misma regla.
   - `move(e)` la saca del handler.
5. **Préstamo de excepción.**
   - No se devuelve, no se guarda donde sobreviva a la llamada y no se captura.
   - Un local de la misma llamada hereda el préstamo.
   - Declarar el parámetro `owned` lo convierte en dueño.
6. **Local `pin`.**
   - Una función async con un local `pin` y alguna suspensión es error.
   - Un préstamo cuya fuente es un parámetro o un campo no cruza una suspensión; lo rechaza la generación del
     frame (regla del MIR; no se midió en esta revisión).

### Límites

Lo siguiente no está garantizado. Los límites 1–3 y 5 se midieron con sondas en `-O0` y `-O2`; el 4 sale del
diseño del análisis.

1. **Referencias no `owned` de parámetros o campos.** Guardar un parámetro en un static y destruir después el
   objeto deja una referencia colgante y compila. Es responsabilidad del programador.
2. **Préstamos derivados de un local `raii` o `scope`.** Un campo, un getter, una llamada o un `?:` sobre el
   local (`return b.Name;`, `s = b.Name;` hacia un local exterior, `h.S = b.Name;`) compila y puede sobrevivir al
   owner. Sólo se rechaza el alias directo del owner.
3. **Préstamos derivados de una excepción atrapada.** `m = p.Text;` hacia un local exterior compila y sobrevive
   al handler.
4. **Análisis conservador de llamadas.** El análisis no sabe de qué entrada viene el resultado de una llamada.
   Un resultado referencia o string no `owned` con un receptor o argumento prestado, o con un temporal owned
   entregado a un parámetro que no toma ownership, se trata como préstamo aunque en realidad no lo sea.
5. **Ramas con suspensión.** Un préstamo que cruza un `?:` o `??` dentro de una expresión completa que suspende
   necesita un dueño puntero. Si no lo tiene, el temporal conserva la conducta anterior: se fuga, sin referencia
   colgante. La sonda con un `await` en la misma sentencia lo destruye una vez.

## EN — Normative contract

### Guarantee

NativeSharp prevents leaks, double destruction and escapes when the source and its life are known statically.
Non-owned references that come from parameters or fields remain the programmer's responsibility.

The guarantee covers exactly the sources and sinks below. Anything not on this list is not guaranteed; the
limits are listed at the end.

### Known-life sources

| Source | Its life ends |
|---|---|
| Owned temporary: the result of an `owned`-returning call, or `new` of a class | at the end of the full expression |
| `raii` local | when its scope exits |
| `scope` object | when its scope exits; it lives in the frame |
| Caught exception (the `catch` variable and its aliases in the same handler) | when the handler ends |
| Exception loan (a non-`owned` exception parameter and its aliases in the same call) | when the call returns |
| `pin` local | it does not cross a suspension |

A single compiler classifier (`KnownLifeSources`) recognizes these sources. Every rule below asks it and keeps
its own diagnostic.

### Rules

1. **Owned temporary.**
   - It is destroyed exactly once, at the end of the full expression, after every other use and in reverse order
     of creation.
   - If a `?:` or `??` did not make it on the path taken, it is not destroyed.
   - A borrow of the temporary cannot initialize a local, be assigned, be returned or be yielded. A borrow is:
     - a reference or string field read on the temporary or on another borrow;
     - such a getter;
     - the reference or string, non-`owned` result of a call (method, function or delegate) whose receiver
       or some argument is a borrow, or whose argument is the owned temporary itself handed to a parameter
       that does not take ownership (`Id(MakeText())`);
     - a `?:` or `??` with some branch that is one.
   - Reading a copyable value from the temporary is not a borrow.
   - If the borrow ends in a plain value, in a new owner (a new string, the object returned by a method called on
     the borrow) or in a field store, the temporary lives until the end of the expression.
   - A temporary lent as an argument also lives until the end of the full expression, so
     `Id(MakeText()).Length` reads it while it is alive.
   - `(new T(...)).Method()` is an error: the object has no owner.
2. **`raii` local.** It is not copied to a non-owning local, by assignment or in a capture without `move`. It is
   not returned without `return move(...)`.
3. **`scope` object.**
   - It is not stored in a field or a static. Naming the object on the right-hand side is enough.
   - It is not returned or captured.
   - Returning a value read from it (`return b.Id;`) is valid.
4. **Caught exception.**
   - It is not returned, not stored where it outlives the handler, and not captured.
   - A local of the same handler may hold it and falls under the same rule.
   - `move(e)` takes it out of the handler.
5. **Exception loan.**
   - It is not returned, not stored where it outlives the call, and not captured.
   - A local of the same call inherits the loan.
   - Declaring the parameter `owned` makes it an owner.
6. **`pin` local.**
   - An async function with a `pin` local and any suspension is an error.
   - A borrow whose source is a parameter or a field does not cross a suspension; frame generation rejects it
     (an MIR rule; not measured in this revision).

### Limits

The following is not guaranteed. Limits 1–3 and 5 were measured with probes at `-O0` and `-O2`; limit 4 follows
from the design of the analysis.

1. **Non-`owned` references from parameters or fields.** Storing a parameter in a static and then destroying
   the object leaves a dangling reference and compiles. It is the programmer's responsibility.
2. **Borrows derived from a `raii` or `scope` local.** A field, a getter, a call or a `?:` on the local
   (`return b.Name;`, `s = b.Name;` into an outer local, `h.S = b.Name;`) compiles and can outlive the owner.
   Only a direct alias of the owner is rejected.
3. **Borrows derived from a caught exception.** `m = p.Text;` into an outer local compiles and outlives the
   handler.
4. **Conservative call analysis.** The analysis does not know which input a call result comes from. A
   non-`owned` reference or string result with a borrowed receiver or argument, or with an owned temporary
   handed to a parameter that does not take ownership, is treated as a borrow even when it is not one.
5. **Branches with a suspension.** A borrow that crosses a `?:` or `??` inside a full expression that suspends
   needs a pointer owner. Without one, the temporary keeps the earlier behavior: it leaks, with no dangling
   reference. The probe with an `await` in the same statement destroys it once.

## Referencias / References

- Elementos 131a, 131b, 135, 135a y 136 del plan de congelamiento: `docs/3.0/decisiones-congelamiento/51-*`,
  `52-*`, `54-*`, `58-*` y `55-*`.
- DEC-10 y P10 (excepciones): `docs/3.0/decisiones-congelamiento/` y `Freeze30ContractTests`.
- Sondas: `A:\temp\nsc-dec\d37`, `d38`, `d40`, `d41`, `d42` y `d43`.
