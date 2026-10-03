# NativeSharp 3.0 — Drop glue genérico e indirecto / Generic and indirect drop glue

## Contrato normativo / Normative contract

Todo tipo valor no trivial tiene una única identidad física de destrucción derivada de su identidad
nominal cerrada. Su firma ABI es `void(ptr storage)`. El storage pertenece al caller; el thunk
destruye su contenido vivo, pero no libera el bloque exterior.

Every non-trivial value type has one physical destruction identity derived from its closed nominal
identity. Its ABI signature is `void(ptr storage)`. The caller owns the storage; the thunk destroys
its live contents but does not release the outer block.

El productor define el thunk porque sólo él conoce destructores privados, layout y decisiones de
especialización. Un consumidor importado declara y llama ese símbolo; no sintetiza una receta
aproximada a partir de la superficie pública.

The producer defines the thunk because only it knows private destructors, layout, and specialization
decisions. An importing consumer declares and calls that symbol; it does not synthesize an
approximate recipe from the public surface.

## Orden y destrucción única / Order and exactly-once destruction

- Campos de struct se destruyen en orden inverso a su declaración.
- Arrays fijos se destruyen desde el último elemento inicializado al primero.
- Un variant lee su tag y destruye solamente el payload activo.
- Un field `owned` se pone en null antes de destruir el objeto o array apuntado.
- Una bandera de vida se limpia antes de llamar el thunk cuando el valor completo puede moverse.

- Struct fields are destroyed in reverse declaration order.
- Fixed arrays are destroyed from the last initialized element to the first.
- A variant reads its tag and destroys only the active payload.
- An `owned` field is cleared before destroying the referenced object or array.
- A liveness flag is cleared before calling the thunk when the complete value can be moved.

Estas reglas hacen idempotente el cleanup respecto a un move ya registrado: el destino conserva la
única obligación y el origen no vuelve a ejecutar el thunk.

These rules make cleanup idempotent with respect to a recorded move: the destination retains the
sole obligation and the source does not invoke the thunk again.

## Genéricos, arrays y variants / Generics, arrays, and variants

Cada especialización cerrada obtiene identidad propia. Un array no inventa otro ABI: recorre sus
elementos y llama el thunk nominal de cada elemento que lo necesite. El union privado que porta los
payloads de un variant no se destruye por separado; el thunk del variant es el único dueño del tag.

Each closed specialization gets its own identity. An array does not invent another ABI: it walks
its elements and calls each required nominal element thunk. The private union carrying variant
payloads is not destroyed independently; the variant thunk is the sole owner of the tag.

## Frontera de módulo / Module boundary

NSMETA 1.36 publica `dropGlueEntryName` sólo para valores no triviales. El nombre debe coincidir con
la derivación canónica de la identidad del tipo; un valor con storage `owned` sin thunk se rechaza.
La identidad participa en las huellas API/ABI y en la identidad de caché MIR.

NSMETA 1.36 publishes `dropGlueEntryName` only for non-trivial values. The name must match the
canonical derivation from the type identity; a value with `owned` storage and no thunk is rejected.
The identity participates in API/ABI fingerprints and MIR cache identity.

Metadata 1.35 y anterior sigue siendo legible. Esos productores no prometen un thunk valor; un
programa que necesite el nuevo contrato debe recompilar el productor con NSMETA 1.36.

Metadata 1.35 and earlier remains readable. Those producers do not promise a value thunk; a program
requiring the new contract must rebuild the producer with NSMETA 1.36.

## Límites / Limits

F08 no define descarga segura de DLL con valores vivos. La vida del módulo, contadores de objetos y
política de unload pertenecen a T13-F08. Un `union` C crudo continúa prohibiendo ownership porque no
tiene tag; sólo el carrier generado de un `variant` puede contener payloads propietarios.

F08 does not define safe DLL unloading while values remain alive. Module lifetime, live-object
counts, and unload policy belong to T13-F08. A raw C `union` still forbids ownership because it has
no tag; only a compiler-generated `variant` carrier may contain owning payloads.
