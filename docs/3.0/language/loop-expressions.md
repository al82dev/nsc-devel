# Loop expressions / Expresiones `loop`

## ES — Contrato normativo

Una expresión `loop` repite su bloque hasta que un `break` alcance ese loop. Su forma básica es:

```nativesharp
int answer = loop
{
    if (ready) { break 42; }
};
```

El tipo de la expresión es el tipo común de todos sus `break` alcanzables. La unificación usa las
conversiones implícitas normales y descarta caminos `never`. Sin un `break` alcanzable, el loop es
`never`. `break;` produce `void`; no se puede mezclar con un valor.

Las etiquetas son nombres léxicos explícitos:

```nativesharp
int answer = loop outer
{
    loop inner
    {
        break outer: 42;
    };
};
```

`continue outer;` inicia otra iteración del loop nombrado. No puede haber dos etiquetas activas con
el mismo nombre. Un `break` sin etiqueta conserva la regla habitual: sale del loop o switch más
interno. Para atravesar un switch o loop interior debe nombrarse el destino.

El valor se evalúa antes de salir y antes de ejecutar cleanups. `defer`, RAII y `finally` se
ejecutan exactamente como en cualquier salida estructurada. Los valores move-only se transfieren
explícitamente con `break move(value);`. Un préstamo enraizado en un local creado dentro del loop
no puede convertirse en resultado porque el local muere al salir.

`while`, `do`, `for`, `foreach` y `switch` siguen siendo sentencias; no producen valor y rechazan
`break value`.

## EN — Normative contract

A `loop` expression repeats its block until a `break` reaches that loop. Its type is the common
type of all reachable break values, using ordinary implicit conversions and ignoring divergent
`never` paths. A loop with no reachable break is `never`; a valueless break contributes `void` and
cannot be mixed with a value.

Labels use `loop name { ... }`, `break name: value;`, and `continue name;`. Active labels are
unique. An unlabelled break keeps the ordinary innermost-loop-or-switch rule, so an outer loop must
be named when a nested construct intervenes.

The break value is evaluated before lexical cleanup. `defer`, RAII, and `finally` therefore run
after the result is preserved. Move-only values require `move`, and a borrow rooted in loop-local
storage cannot escape as the loop result. Statement loops and switches remain `void` constructs
and reject value breaks.

HIR and MIR 1.27 expose the loop, its result type, label, exit target, value conversion, and cleanup
path. LLVM lowers it to native control flow and introduces no runtime loop object or allocation.
