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

Un agregado move-only usa la forma explícita `..move(source)`. Sólo se admite en el initializer de
un local y el source debe ser un local move-only nombrado o un parámetro por valor. Los campos no
escritos se transfieren y se vacían en el source; los fields reemplazados permanecen vivos allí. El
compilador rechaza leer una ruta transferida y también impide introducir un alias en un field
`owned` explícito.

El destino se zero-inicializa y registra cleanup antes de evaluar entradas que puedan lanzar. Si
hay unwind, se destruyen los fields ya instalados y el source conserva la responsabilidad de sus
siblings no movidos. Un tipo con destructor personalizado no admite move-spread porque su
destructor podría observar el valor completo después de un movimiento parcial.

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

A move-only aggregate uses the explicit `..move(source)` form. It is accepted only in a local
initializer, and the source must be a named move-only local or by-value parameter. Unwritten fields
are transferred and cleared in the source; explicitly replaced fields remain live there. The
compiler rejects later reads of transferred paths and prevents an explicit `owned` field from
receiving an alias.

The destination is zero-initialized and registers cleanup before any potentially throwing entry is
evaluated. On unwind, installed fields are destroyed while the source retains responsibility for
untouched siblings. A type with a custom destructor cannot use move-spread because that destructor
could observe the complete value after a partial move.
