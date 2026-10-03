# NativeSharp 3.0 — Análisis de escape interprocedural / Interprocedural escape analysis

## Contrato normativo / Normative contract

Cada callable publicable tiene un contrato por parámetro: `NoEscape`, `Return`, `CallerOwned`,
`Global` o `Unknown`. El contrato describe hasta dónde puede llegar una dirección o vista recibida;
no cambia el ABI físico ni agrega runtime.

Every publishable callable has one contract per parameter: `NoEscape`, `Return`, `CallerOwned`,
`Global`, or `Unknown`. The contract describes how far a received address or view may travel; it
does not change the physical ABI or add runtime machinery.

## Clases de escape / Escape classes

- `NoEscape`: el callee sólo observa o muta durante la llamada.
- `Return`: el valor retornado puede conservar la procedencia del parámetro.
- `CallerOwned`: el callee escribe mediante storage explícitamente propiedad del caller.
- `Global`: el callee retiene la dirección en estado, closure o storage de vida mayor.
- `Unknown`: no existe prueba suficiente; imports externos, contratos antiguos y genéricos no
  cerrados usan este fallback.

- `NoEscape`: the callee only observes or mutates during the call.
- `Return`: the returned value may preserve the parameter's provenance.
- `CallerOwned`: the callee writes through storage explicitly owned by the caller.
- `Global`: the callee retains the address in state, a closure, or longer-lived storage.
- `Unknown`: proof is unavailable; external imports, old contracts, and non-closed generics use
  this fallback.

Cuando un callable hace varias cosas, se publica la clase más conservadora:
`Unknown > Global > CallerOwned > Return > NoEscape`.

When a callable performs several operations, the most conservative class is published:
`Unknown > Global > CallerOwned > Return > NoEscape`.

## Propagación y diagnóstico / Propagation and diagnostics

El análisis termina todos los cuerpos alcanzables y después propaga summaries por el grafo de
llamadas hasta punto fijo. Por ello un wrapper que reenvía a un callee `Global` también publica
`Global`, aunque no contenga un store directo.

Analysis completes all reachable bodies and then propagates summaries through the call graph to a
fixed point. A wrapper forwarding to a `Global` callee therefore publishes `Global` even without a
direct store.

En edición 3, pasar storage del frame a un parámetro `Global` es error causal. `Unknown` falla
cerrado para referencias, pins, vistas y VLAs con lifetime administrado por el compilador. Un
puntero crudo ordinario mantiene el modelo explícito/FFI histórico; el compilador no lo presenta
como préstamo seguro.

In edition 3, passing frame storage to a `Global` parameter is a causal error. `Unknown` fails
closed for references, pins, views, and VLAs whose lifetime is compiler-managed. An ordinary raw
pointer keeps the historical explicit/FFI model; the compiler does not present it as a safe borrow.

## Frontera de módulo / Module boundary

NSMETA 1.35 almacena un byte validado por parámetro. El número de entradas debe coincidir con la
firma; valores, conteos o catálogos inválidos se rechazan. Metadata 1.34 y anterior sigue siendo
legible y se hidrata conservadoramente como `Unknown` donde el consumidor necesite una respuesta.

NSMETA 1.35 stores one validated byte per parameter. Entry count must match the signature; invalid
values, counts, or catalogues are rejected. Metadata 1.34 and earlier remains readable and is
conservatively hydrated as `Unknown` whenever the consumer needs an answer.

El catálogo forma parte de la huella pública: cambiar `NoEscape` por `Global`, o cualquier otra
clase, cambia el fingerprint y obliga a invalidar consumidores incrementales.

The catalogue is part of the public fingerprint: changing `NoEscape` to `Global`, or any other
class, changes the fingerprint and forces incremental consumers to invalidate their result.

## Límites / Limits

F07 no introduce atributos de usuario para mentir sobre código externo. Los contratos explícitos
de callbacks, function types y captures de T08 podrán refinar `Unknown`; hasta entonces el fallback
es deliberadamente conservador. El análisis no convierte punteros crudos en referencias seguras.

F07 does not introduce user attributes that could lie about external code. T08 callback, function
type, and capture contracts may refine `Unknown`; until then the fallback is deliberately
conservative. Analysis does not turn raw pointers into safe references.
