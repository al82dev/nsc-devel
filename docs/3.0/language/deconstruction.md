# Deconstrucción / Deconstruction

## Español

NativeSharp 3.0 permite declarar variables y asignar almacenamiento existente mediante patrones
de tupla:

```nativesharp
var (x, (y, _)) = source;
(left, right) = (right, left);
```

La declaración crea una variable inferida por cada nombre hoja. `_` descarta su posición. La
asignación no declara nombres: cada hoja debe ser almacenamiento escribible y direccionable. Los
patrones pueden ser `()`, `(x,)`, tener cualquier aridad soportada y anidarse; cada nivel debe tener
la misma forma que la tupla correspondiente.

La fuente puede ser una tupla o un tipo nominal con exactamente un método accesible de esta forma:

```nativesharp
public (int, int) Deconstruct();
```

El protocolo se resuelve estáticamente. `Deconstruct` debe ser un método de instancia,
paramétrico-cero, no virtual ni abstracto, y devolver una tupla por valor. Un receptor readonly
requiere un método readonly. No hay reflexión, búsqueda dinámica ni una llamada implícita por cada
elemento.

El compilador evalúa la fuente una sola vez y materializa la tupla completa antes de calcular una
dirección destino. Después escribe las hojas de izquierda a derecha. Por ello los swaps y el
aliasing conservan el valor original, mientras que los efectos de los destinos tienen orden
textual. Una fuente `ref` se lee una vez; `Deconstruct` no puede devolver por `ref`.

Hasta que T06 cierre movimientos parciales y drop flags, una tupla con elementos owning/move-only
se rechaza. Un valor nominal move-only sí puede usar `Deconstruct` desde almacenamiento existente,
pero no desde un temporal que exigiría una copia oculta. El frontend valida todas las hojas y sus
conversiones antes de producir HIR, por lo que un fallo intermedio no deja lowering parcial.

## English

NativeSharp 3.0 supports both inferred declarations and assignments through tuple patterns:

```nativesharp
var (x, (y, _)) = source;
(left, right) = (right, left);
```

A declaration creates one inferred local per named leaf. `_` discards its position. An assignment
creates no names: every leaf must be writable, addressable storage. Patterns may be `()`, `(x,)`,
use any supported arity, and nest; every level must match the corresponding tuple shape.

The source may be a tuple or a nominal type with exactly one accessible instance `Deconstruct()`
method that has no parameters, is non-virtual and non-abstract, and returns a tuple by value. The
protocol is resolved statically; there is no reflection or dynamic search. A readonly receiver
requires a readonly method.

The source is evaluated once and the complete tuple is materialized before any destination address
is computed. Leaves are then written left-to-right. This makes swaps and aliasing snapshot-safe and
gives destination side effects a textual order. A ref-returning source is read once, while a
ref-returning `Deconstruct()` method is rejected.

Until T06 supplies partial moves and drop flags, tuple results containing owning/move-only elements
are rejected. An existing move-only nominal value may be a receiver without being copied, but a
temporary that would require hidden copying is rejected. All leaves and conversions are validated
before HIR is emitted, so an intermediate failure cannot produce partial lowering.
