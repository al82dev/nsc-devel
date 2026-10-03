# Objetos trait explícitos / Explicit trait objects

## Contrato / Contract

ES: `dyn Trait` borra el tipo concreto de una referencia y conserva únicamente el contrato
nominal del trait. En Windows x64 su representación declarada es una referencia de una palabra
(8 bytes, alineación 8). La palabra apunta al objeto NativeSharp existente; el descriptor del
objeto contiene el mapa de interfaces y selecciona la tabla del trait. No es un fat pointer de dos
palabras y convertir a `dyn` no reserva memoria, no copia el objeto y no crea una caja.

EN: `dyn Trait` erases the concrete type of a reference while retaining the nominal trait
contract. On Windows x64 its declared representation is a one-word reference (8-byte size and
alignment). The word points at the existing NativeSharp object; its descriptor owns the interface
map used to select the trait table. It is not a two-word fat pointer, and conversion to `dyn` does
not allocate, copy, or box the object.

## Object safety

A trait used after `dyn` must have a stable instance-dispatch surface. Generic methods and
associated types/constants without defaults make the trait non-object-safe. The diagnostic is
issued at the `dyn` use, so a trait may still exist for static constraints even when it cannot be
erased. Pointer and array modifiers around `dyn Trait` are rejected: the trait object already is a
reference carrier.

## Ownership, null y conversión / Ownership, null, and conversion

- `borrowed dyn Trait` is a non-owning view and follows the source borrow lifetime.
- `owned dyn Trait` transfers ownership of the existing object. Destruction uses the concrete
  descriptor exactly once.
- `dyn Trait` may be null; the null representation is the zero pointer.
- A class reference may upcast to a trait it implements, and a derived trait object may upcast to
  a base trait. Downcasts are not implicit.
- Struct values are not silently boxed. A value-to-`dyn` conversion is rejected unless a future
  explicit owning wrapper defines storage and lifetime.

## Metadata and DLL boundary

NSMETA 1.31 publishes object-safety, descriptor-map dispatch, zero-null semantics, one-word
layout, and the no-allocation conversion rule. It also preserves the `dyn` marker on member,
namespace-function, delegate, field, and namespace-state signatures. Removing `dyn` is a source
contract change, but it is not by itself a physical ABI change because the nominal interface
reference has the same Windows x64 carrier. Producer and consumer therefore reconstruct the same
source contract across `.lib`/`.dll`/`.nsmeta` boundaries.

## Example

```nativesharp
trait IScore { int Score(); }

class Number : IScore
{
    public int Score() { return 42; }
}

borrowed Number value = new Number();
borrowed dyn IScore view = value; // no allocation and no box
int result = view.Score();
```
