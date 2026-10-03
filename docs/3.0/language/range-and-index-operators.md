# NativeSharp 3.0 — Rangos e índices / Range and index operators

## Español

NativeSharp 3.0 usa rangos **half-open**: `inicio..fin` incluye `inicio` y excluye `fin`.
`vista[1..4]` contiene tres elementos. Un extremo omitido significa el principio o el final:

| Forma | Inicio resuelto | Fin resuelto |
|---|---:|---:|
| `vista[..]` | `0` | `Length()` |
| `vista[inicio..]` | `inicio` | `Length()` |
| `vista[..fin]` | `0` | `fin` |
| `vista[inicio..fin]` | `inicio` | `fin` |
| `vista[^n..^m]` | `Length() - n` | `Length() - m` |

`^n` cuenta desde el final. En consecuencia, `^1` nombra el último elemento cuando se usa como
índice, mientras `^0` nombra la posición **uno después del último**: es válido como extremo de un
rango vacío (`vista[^0..^0]`) y falla como acceso a un elemento (`array[^0]`). Una distancia
constante negativa se rechaza al compilar.

### Protocolo estructural

`receiver[start..end]` no construye un objeto `Range` ni asigna memoria. El tipo concreto del
receptor debe publicar exactamente estos métodos de instancia accesibles:

```text
int Length()
TResult Slice(int start, int length)
```

La resolución es estática. Los miembros de protocolo `virtual`, `override`, `abstract` o de una
interfaz no se aceptan: el azúcar no introduce despacho oculto. `Length()` devuelve por valor y
`Slice` también. Los `Span<T>`, `ReadOnlySpan<T>` y `StringView` de Core ya tienen esta forma.
Tipos de usuario pueden participar sin heredar ni implementar una interfaz nominal.

El receptor se evalúa exactamente una vez. El compilador reutiliza ese valor para `Length()` y
`Slice(start, end - start)`. También conserva el orden de evaluación de los extremos.

Antes de llamar a `Slice`, el modo seguro comprueba `0 <= start <= end <= Length()`. Los rangos
que el compilador demuestra válidos para toda longitud conforme —`0..0`, `0..Length` y
`Length..Length`— no emiten ese guard ni siquiera en O0; los restantes dejan una condición
explícita que la optimización Release puede eliminar si demuestra sus límites. No hay boxing,
carrier de rango, copia ni allocation implícitos.

Un arreglo heap y un string nativo no se rebanan directamente: hacerlo produciría una copia y una
asignación invisibles. Para ellos se crea primero una vista (`Span<T>`, `ReadOnlySpan<T>` o
`StringView`). La indexación de elemento sí admite `^n`, incluidos índices rectangulares por
dimensión.

## English

NativeSharp 3.0 ranges are **half-open**: `start..end` includes `start` and excludes `end`.
`view[1..4]` therefore contains three elements. An omitted endpoint means the beginning or end:

| Form | Resolved start | Resolved end |
|---|---:|---:|
| `view[..]` | `0` | `Length()` |
| `view[start..]` | `start` | `Length()` |
| `view[..end]` | `0` | `end` |
| `view[start..end]` | `start` | `end` |
| `view[^n..^m]` | `Length() - n` | `Length() - m` |

`^n` counts from the end. Thus `^1` names the last element when used as an index, while `^0`
names the **one-past-the-end** position: it is valid as an empty-range endpoint
(`view[^0..^0]`) and fails as an element access (`array[^0]`). A negative constant distance is
rejected at compile time.

### Structural protocol

`receiver[start..end]` does not construct a `Range` object or allocate memory. The receiver's
concrete type must expose these accessible instance methods exactly:

```text
int Length()
TResult Slice(int start, int length)
```

Resolution is static. `virtual`, `override`, `abstract`, and interface protocol members are not
accepted, so the sugar cannot introduce hidden dispatch. Both methods return by value. Core's
`Span<T>`, `ReadOnlySpan<T>`, and `StringView` already have this shape. User types can participate
without inheritance or a nominal interface.

The receiver is evaluated exactly once. The compiler reuses that value for `Length()` and
`Slice(start, end - start)`, while preserving endpoint evaluation order.

Before calling `Slice`, safe lowering checks `0 <= start <= end <= Length()`. Ranges proven valid
for every conforming length —`0..0`, `0..Length`, and `Length..Length`— emit no such guard even at
O0; remaining explicit conditions can be removed by Release optimization when their bounds are
proven. No boxing, range carrier, copy, or implicit allocation is introduced.

Heap arrays and native strings cannot be sliced directly because that would hide a copy and an
allocation. Create a view (`Span<T>`, `ReadOnlySpan<T>`, or `StringView`) first. Element indexing
does accept `^n`, including a separate from-end decision for every rectangular-array dimension.
