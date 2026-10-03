# Extensiones estáticas / Static extension methods

## Contrato / Contract

ES: Un bloque `extension` declara funciones de namespace que pueden invocarse con sintaxis de
miembro sin modificar el tipo receptor. La declaración conserva una forma estática ordinaria: no
añade campos, slots, vtables ni identidad al tipo extendido, y la llamada no crea un objeto
intermedio.

EN: An `extension` block declares namespace functions that may be invoked with member syntax
without modifying the receiver type. The declaration retains an ordinary static-function form: it
adds no fields, slots, vtables, or identity to the extended type, and the call creates no wrapper
object.

```nativesharp
namespace Geometry.Extensions
{
    extension ref Point
    {
        public int Translate(int amount)
        {
            this.X = this.X + amount;
            return this.X;
        }
    }
}

using Geometry.Extensions;
Point point;
point.X = 40;
int result = point.Translate(2);
```

## Receptores / Receivers

- `extension T` receives a value. Normal implicit conversions and overload ranking apply.
- `extension ref T` receives a mutable address. The expression must be addressable, writable, and
  exactly `T`; numeric or representation-changing conversions are forbidden.
- `extension in T` receives a readonly address. The expression must be addressable and exactly
  `T`.
- Inside the block, `this` denotes the receiver. It is not a class instance and does not create an
  implicit runtime context.
- Extension blocks contain namespace-function declarations only. Accessibility belongs to each
  function, not to the block.

## Lookup y prioridad / Lookup and priority

1. Native fields, properties, events, methods, and compiler-provided root members are considered
   first. A real compatible member always wins.
2. Extension candidates come only from the current lexical namespace and namespaces named by an
   explicit `using` visible at the call site.
3. The receiver type's namespace is not searched implicitly. NativeSharp therefore has no
   argument-dependent extension lookup.
4. All visible extension candidates with the written name enter one overload set. Receiver and
   explicit arguments are checked together; source-file order does not participate.
5. If two equally good candidates remain, compilation reports an explicit ambiguity. Importing
   another namespace can therefore expose a conflict, never silently change selection by textual
   order.

ES: Una extensión local cuyo receptor no es compatible no oculta una extensión importada
compatible. Las reglas se aplican también a tipos primitivos, structs, clases, enums y tipos
genéricos cerrados.

EN: An incompatible local extension does not hide a compatible imported extension. The same rules
apply to primitive types, structs, classes, enums, and closed generic types.

## Forma estática y costos / Static form and costs

La implementación se baja a una llamada directa de función de namespace. El receptor se convierte
en el primer argumento físico; para `ref`/`in` se pasa su dirección. No existe despacho dinámico,
registro global, reflexión, allocation ni búsqueda en runtime. La función también conserva su
identidad estática para metadata, linker y depurador.

The implementation lowers to a direct namespace-function call. The receiver becomes the first
physical argument; `ref`/`in` pass its address. There is no dynamic dispatch, global registry,
reflection, allocation, or runtime lookup. The function also retains its static identity for
metadata, the linker, and the debugger.

## Módulos y NSMETA / Modules and NSMETA

NSMETA 1.32 marks public namespace functions that are extensions. Parameter zero stores the
receiver type and its value/ref/in contract. A consumer reconstructs the `extension` block, omits
the synthetic receiver from the written parameter list, validates the producer's physical symbol,
and links the ordinary native function. Metadata 1.31 cannot encode this lookup identity and must
reject such a downlevel write instead of publishing a misleading free function.

## Recomendaciones de API / API guidance

- Put extensions in a narrowly named namespace and require an explicit `using` from consumers.
- Prefer a real member when the type owner controls the API and the operation is intrinsic to the
  type.
- Avoid common names such as `Get`, `Read`, or `Convert` across broad extension namespaces; they
  increase deliberate ambiguity at import sites.
- Use `ref` only when mutation of the receiver is part of the public contract. Use `in` for a
  stable readonly address and value for small copyable receivers.
- Do not use extensions to imitate inheritance, hidden state, or dynamic dispatch; traits and real
  members express those contracts.
