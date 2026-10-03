# NativeSharp 3.0 — Parámetros variádicos tipados / Typed parameter packs

## Español

`params ReadOnlySpan<T>` permite escribir cero, uno o varios argumentos `T` sin construir un
arreglo de heap:

```nativesharp
int Sum(params ReadOnlySpan<int> values) { ... }

Sum();
Sum(42);
Sum(20, 22);
```

Cuando la llamada expande elementos, el caller reserva un arreglo temporal y el carrier del span
en su stack. Evalúa y convierte cada elemento de izquierda a derecha, guarda `{puntero, count}` y
llama a la firma física ordinaria que recibe un `ReadOnlySpan<T>`. Una llamada con un span ya
construido lo pasa directamente y sirve para forwarding sin volver a empaquetar.

El almacenamiento temporal dura sólo lo que dura el frame del caller. Una vista obtenida de una
expansión no puede retornarse ni escapar a almacenamiento de mayor vida. Los elementos `owned`,
`move(...)`, fresh owners y parámetros `ref/out/in` tampoco se pueden introducir en el pack: el
span es readonly y no adquiere ownership.

No hay arreglo administrado, llamada al allocator ni heap implícito. Por ello la expansión es
válida dentro de un contexto `noheap`. Si una firma fija y una firma `params` son viables, la fija
gana; pasar directamente un `ReadOnlySpan<T>` también gana frente a tratarlo como un solo `T`.

Los packs estáticos de tipos (`Ts...`) son una característica genérica distinta, cubierta por
T04. No son el mecanismo de variadic values de este documento.

## English

`params ReadOnlySpan<T>` accepts zero, one, or many `T` arguments without constructing a heap
array:

```nativesharp
int Sum(params ReadOnlySpan<int> values) { ... }

Sum();
Sum(42);
Sum(20, 22);
```

For an expanded call, the caller reserves a temporary element array and span carrier on its stack.
It evaluates and converts every element from left to right, stores `{pointer, count}`, and calls
the ordinary physical signature that receives `ReadOnlySpan<T>`. A prebuilt span is passed
directly, which supports forwarding without repacking.

Temporary storage lasts only for the caller's frame. A view obtained from an expansion cannot be
returned or escape into longer-lived storage. `owned` elements, `move(...)`, fresh owners, and
`ref/out/in` arguments are also rejected: the readonly span does not acquire ownership.

No managed array, allocator call, or implicit heap operation is emitted, so expansion is legal in
a `noheap` context. When both a fixed signature and a `params` signature are viable, the fixed
signature wins; passing an existing `ReadOnlySpan<T>` also wins over treating it as one `T`.

Static generic type packs (`Ts...`) are a separate T04 feature. They are not the variadic-value
mechanism described here.
