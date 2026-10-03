# Literales y conversiones de tupla / Tuple literals and conversions

## Español

Un literal `(a, b, ...)` evalúa cada expresión exactamente una vez y en orden textual. Su tipo
fuente se infiere con el tipo natural de cada elemento; el contexto de asignación, argumento o
`return` aplica después una conversión implícita elemento por elemento.

```nativesharp
(long, double) value = (MakeInt(), MakeInt());
```

La aridad debe coincidir. Cada posición debe admitir su propia conversión implícita; no existe una
reinterpretación de bytes entre carriers sólo porque sus tamaños sean iguales. Se admiten
conversiones numéricas, identidad de puntero/referencia, upcast de clase, conversión a interfaz y
conversiones de tuplas anidadas. Una conversión de usuario dentro de una tupla se rechaza: exigiría
emitir una llamada por elemento y no pertenece al conversor estructural sin efectos ocultos.

El overload resolution compara primero los rangos de conversión. Si dos destinos tienen la misma
categoría amplia, usa el orden parcial entre destinos: `(long,long)` es mejor destino para
`(int,int)` que `(double,double)` porque el primero convierte al segundo y no al revés. Si ninguna
alternativa domina, la llamada sigue siendo ambigua.

HIR conserva un único literal agregado. LLVM materializa el carrier fuente una vez, carga cada
elemento desde su offset, convierte y escribe el carrier destino. Padding nunca se reinterpreta y
los efectos no se repiten. Un error informa la posición humana del primer elemento incompatible;
una diferencia de aridad informa ambos conteos.

## English

A tuple literal `(a, b, ...)` evaluates each expression exactly once in textual order. Its source
type is inferred from the natural type of every element; an assignment, argument, or `return`
context then applies an implicit conversion element by element.

Arity must match and every position must provide its own implicit conversion. Equal carrier size
never authorizes a byte reinterpretation. Numeric conversions, pointer/reference identity, class
upcasts, interface conversions, and nested tuple conversions are supported. A user-defined
conversion inside a tuple is rejected because it would require a hidden call per element and is
not part of the effect-free structural converter.

Overload resolution compares conversion ranks first. When two targets share a broad category, it
uses the target partial order: `(long,long)` is a better target for `(int,int)` than
`(double,double)` because the former converts to the latter and not conversely. If neither target
dominates, the call remains ambiguous.

HIR retains one aggregate literal. LLVM materializes the source carrier once, loads each element
at its declared offset, converts it, and writes the destination carrier. Padding is never
reinterpreted and effects are never replayed. A failure names the first incompatible element by
one-based position; an arity mismatch reports both counts.
