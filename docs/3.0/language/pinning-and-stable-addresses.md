# NativeSharp 3.0 — Pinning y direcciones estables / Pinning and stable addresses

## Contrato normativo / Normative contract

Pinning es una obligación estática: mientras vive una declaración pinned, el programa no puede
relocalizar, reemplazar ni extraer por movimiento el storage protegido. No existe wrapper de
runtime, handle oculto, registro global ni asignación implícita. La sintaxis requiere
`<Language>3</Language>`.

Pinning is a static obligation: while a pinned declaration is alive, the program may not relocate,
replace, or move out of the protected storage. It introduces no runtime wrapper, hidden handle,
global registry, or implicit allocation. The syntax requires `<Language>3</Language>`.

## Owners pinned / Pinned owners

```nativesharp
pin raii Device stable = new Device();
```

`pin raii T name` conserva el ownership y cleanup léxico de `raii`, pero fija su identidad hasta
la destrucción. El owner puede leerse y prestarse; no puede ser origen de `move`, reemplazarse ni
sufrir un partial move. Su destructor se ejecuta exactamente una vez en el mismo grafo de cleanup
de T06-F05.

`pin raii T name` keeps ordinary `raii` ownership and lexical cleanup while fixing its identity
until destruction. The owner may be read and borrowed; it may not be moved from, replaced, or
partially moved. Its destructor runs exactly once through the T06-F05 cleanup graph.

## Proyecciones pinned / Pinned projections

```nativesharp
pin ref int value = ref stable.Value;
pin ref byte first = ref fixedBuffer[0];
```

`pin ref T` crea una referencia mutable exclusiva cuya dirección debe poder demostrarse estática.
El place canónico y el lifetime son los de la referencia local normal; el marcador pinned se
conserva además en method model, HIR, MIR e identidad de caché.

`pin ref T` creates an exclusive mutable reference whose address must be statically provable. Its
canonical place and lifetime are those of an ordinary reference local; the pinned marker is also
preserved in the method model, HIR, MIR, and cache identity.

Se admiten locals, parámetros, campos y elementos de arrays/punteros con proyección demostrable.
Un índice dinámico, indexer usuario o retorno ref con procedencia ambigua se rechaza: el compilador
no supone que un contenedor conservará la dirección después de una mutación estructural.

Locals, parameters, fields, and provable array/pointer elements are accepted. Dynamic indices,
user indexers, and ambiguous ref-return provenance are rejected: the compiler does not assume a
container preserves element addresses across structural mutation.

## Construcción y destrucción in-place / In-place construction and destruction

```nativesharp
Cell* storage = /* storage con tamaño y alineación suficientes */;
pin raii Cell cell = new (storage) Cell(storage);
```

Un owner pinned puede recibir una construcción placement. El objeto posee su lifetime, pero toma
prestados los bytes suministrados. Al salir del alcance se invoca la entrada object-only del
descriptor: corre el destructor, incluidas bases, sin liberar el storage. Esto permite objetos
autorreferenciales explícitos y buffers FFI sin confundir destrucción con deallocation.

A pinned owner may receive a placement construction. The object owns its lifetime but borrows the
supplied bytes. Scope exit invokes the descriptor's object-only entry: the destructor, including
bases, runs without releasing storage. This supports explicit self-referential objects and FFI
buffers without conflating destruction with deallocation.

El caller sigue siendo responsable de tamaño, alineación y duración del buffer. `stackalloc`
cumple la duración sólo dentro del frame actual; storage externo sigue el contrato del API nativo.

The caller remains responsible for buffer size, alignment, and lifetime. `stackalloc` is valid
only within the current frame; external storage follows the native API contract.

## Closures, generadores y coroutines / Closures, generators, and coroutines

Una `pin ref` no puede capturarse en una closure porque el entorno podría sobrevivir al storage.
HIR/MIR conservan `pinned=true` para que los lowerings de generadores y coroutines de T09/T10
puedan exigir un frame pinned cuando introduzcan referencias internas. F06 define y transporta la
obligación; no habilita por sí sola `yield` o `async` antes de sus tandas.

A `pin ref` cannot be captured by a closure because the environment could outlive its storage.
HIR/MIR preserve `pinned=true` so the T09/T10 generator and coroutine lowerings can require pinned
frames when they introduce internal references. F06 defines and transports the obligation; it
does not independently enable `yield` or `async` before their own batches.

## Diagnósticos mínimos / Required diagnostics

- sintaxis pinned fuera de edición 3;
- `move`, replacement o partial move de un owner pinned;
- `pin ref` sobre una proyección dinámica o de procedencia desconocida;
- conflictos con otro préstamo mutable solapado;
- escape de `pin ref` por retorno o captura almacenable;
- placement con tipo, tamaño/lifetime o constructor incompatibles.
