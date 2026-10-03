# Spreads de agregados / Aggregate spreads

## Español

Un spread completa los campos no escritos de un `struct`, `record struct`, tupla o arreglo fijo a
partir de exactamente un valor del mismo tipo:

```nativesharp
Pair changed = { X = 2, ..source };
```

Los campos explícitos tienen precedencia sin importar si aparecen antes o después del spread. La
fuente se evalúa una sola vez, exactamente en su posición textual; las demás expresiones también se
evalúan una vez y de izquierda a derecha. Sólo puede existir una fuente y cada campo explícito sólo
puede nombrarse una vez.

HIR publica `aggregate-spread source-evaluations=1`. MIR publica `aggregate-spread-store` con un
descriptor por campo faltante y offsets separados de origen/destino. LLVM reserva el destino final,
lo inicializa y copia únicamente esos campos; no copia padding como semántica de lenguaje ni crea
un objeto runtime. Por ello el camino de tipos `Copy` es válido dentro de funciones `noheap`.

Una copia por spread no concede acceso a la representación privada: puede copiar el valor completo
de un tipo que ya posee, pero un campo escrito explícita o posicionalmente debe ser accesible en el
contexto actual. La fuente debe tener identidad nominal exacta; dos structs con el mismo layout no
son intercambiables.

Los agregados con destructor o campos `owned` permanecen rechazados por ahora. Habilitar
`..move(source)` correctamente exige estados campo por campo, drop flags para values, cleanup en
todos los exits y drop glue genérico/importado. Esos contratos pertenecen a T06-F01, F02 y F08. El
rechazo actual ocurre antes de HIR, por lo que no puede producir aliasing, doble destrucción ni
leaks silenciosos. Al cerrar T06 se activará la mitad move-only y se marcará F06 como terminada.

## English

A spread fills the unwritten fields of a `struct`, `record struct`, tuple, or fixed array from
exactly one value of the same type:

```nativesharp
Pair changed = { X = 2, ..source };
```

Explicit fields take precedence whether they appear before or after the spread. The source is
evaluated once at its textual position; every other expression is also evaluated once and from
left to right. There may be only one source, and an explicit field may be named only once.

HIR exposes `aggregate-spread source-evaluations=1`. MIR exposes `aggregate-spread-store` with one
descriptor per missing field and distinct source/destination offsets. LLVM reserves the final
destination, initializes it, and copies only those fields; padding is not language-level state and
no runtime object is introduced. The `Copy` path is therefore valid in `noheap` functions.

A spread copy does not grant private representation access: code may copy a complete value it
already owns, but a field written explicitly or positionally must be accessible in the current
context. The source requires exact nominal identity; layout-equivalent structs are not
interchangeable.

Aggregates with a destructor or `owned` fields remain rejected for now. Correct `..move(source)`
support requires field states, value drop flags, cleanup on every exit, and generic/imported drop
glue. Those contracts belong to T06-F01, F02, and F08. Rejection currently happens before HIR, so
it cannot create aliasing, double destruction, or silent leaks. F06 will become complete when T06
activates its move-only half.
