# `never` and diverging expressions / `never` y expresiones divergentes

## Español

`never` es el tipo inferior interno de NativeSharp 3: describe una expresión que no produce un
valor porque el control no continúa. No se escribe para declarar objetos y no tiene tamaño,
alineación, representación ABI ni valor por defecto.

`panic()` produce `never`. El compilador puede convertirlo a cualquier tipo de resultado sin
generar bits falsos. Por ejemplo, `stop ? panic() : 42` tiene tipo `int`; la rama divergente no
entra al PHI ni aporta ownership. Un argumento `panic()` hace viable cualquier tipo de parámetro,
pero no resuelve por sí solo una sobrecarga entre destinos no relacionados.

Los locales inferidos desde una expresión divergente son hechos de flujo, no almacenamiento. No
reciben `alloca`, inicialización, drop flag ni ubicación de depuración. El código posterior sigue
siendo revisado para producir diagnósticos útiles, aunque no sea alcanzable durante la ejecución.

El compilador demuestra divergencia para `while (true)`, `do ... while (true)`, `for (;;)` y
`for (; true; ...)` cuando no existe un `break` alcanzable. Un `break` restaura un sucesor normal.
Los brazos de `match` sentencia pueden divergir; la expresión `match` que produce valores pertenece
al contrato separado NS3-T03-F03.

## English

`never` is NativeSharp 3's internal bottom type. It describes an expression that produces no
value because control does not continue. It cannot be used to declare objects and has no size,
alignment, ABI representation, or default value.

`panic()` produces `never`. The compiler may coerce it to any result type without generating fake
bits. For example, `stop ? panic() : 42` has type `int`; the divergent branch contributes neither
a PHI input nor ownership. A `panic()` argument is viable for every parameter type but cannot by
itself choose between unrelated overload targets.

Locals inferred from a divergent expression are flow facts rather than storage. They receive no
alloca, initialization, drop flag, or debug location. Source after divergence is still bound and
checked so unreachable code does not conceal useful diagnostics.

The compiler proves `while (true)`, `do ... while (true)`, `for (;;)` and
`for (; true; ...)` divergent when no reachable `break` exists. A reachable break restores a
normal successor. Statement-match arms may diverge; value-producing match expressions belong to
the separate NS3-T03-F03 contract.
