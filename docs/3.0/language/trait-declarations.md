# Declaraciones `trait` / Trait declarations

## Español

Un `trait` declara comportamiento nominal sin reservar almacenamiento ni crear instancias por sí
mismo. La implementación puede incluir el trait en la lista base de una clase o separarse mediante
`impl Trait for Type`; la segunda forma aplica la coherencia sellada descrita en
[trait-implementations.md](trait-implementations.md).

```nativesharp
public trait IScore
{
    int Score();
    int Bonus() { return Score() + 2; }
    public static int Zero { get; }
}

public class Number : IScore
{
    private int value;
    public Number(int source) { value = source; }
    public int Score() { return value; }
    public static int Zero { get { return 0; } }
}
```

- Un método terminado en `;` es un requisito.
- Un método con cuerpo es un default. Se usa cuando la clase no aporta una implementación propia.
- Dos defaults heredados para la misma firma son ambiguos; un trait derivado debe redeclarar el
  miembro para resolver el conflicto.
- Una propiedad de trait debe ser `static` y sólo declara accessors requeridos. Las propiedades de
  instancia se rechazan para que un trait no introduzca layout.
- Un operador de trait es un requisito `static` sin cuerpo. Los operadores con implementación se
  declaran en el tipo concreto.
- Los métodos estáticos ordinarios con cuerpo y los métodos genéricos dentro del trait permanecen
  fuera de F01; el compilador los rechaza con diagnóstico explícito.
- `where T : Trait` conserva el despacho estático del tipo concreto. `dyn Trait` solicita despacho
  dinámico explícito sobre el ABI de interfaces nativo existente.

Un trait no es una interfaz CLR: NativeSharp 3.0 es AOT nativo Windows x64, no requiere metadata
.NET ni runtime administrado. Tampoco es una clase abstracta: no tiene campos, constructor, objeto
propio ni vtable de clase. Su contrato público se publica en NSMETA 1.28, incluido el orden de slots
de métodos; cambiar ese orden es un cambio de compatibilidad observable para defaults
precompilados.

## English

A `trait` declares nominal behavior without reserving storage or producing instances by itself. A
class may list the trait as a base or use a separate `impl Trait for Type` block; the latter follows
the sealed-coherence contract in [trait-implementations.md](trait-implementations.md).

- A method ending in `;` is a requirement.
- A method with a body is a default used when the class provides no implementation.
- Competing inherited defaults for the same signature are ambiguous; a derived trait must
  redeclare the member to resolve the conflict.
- A trait property must be `static` and declares required accessors only. Instance properties are
  rejected so a trait cannot introduce object layout.
- A trait operator is a bodyless `static` requirement. Its implementation belongs to the concrete
  type.
- Ordinary static methods with bodies and generic trait methods remain outside F01 and are rejected
  explicitly.
- `where T : Trait` keeps concrete static dispatch. `dyn Trait` requests explicit dynamic dispatch
  over NativeSharp's existing native interface ABI.

A trait is not a CLR interface: NativeSharp 3.0 is native Windows x64 AOT and requires neither .NET
metadata nor a managed runtime. A trait is not an abstract class either: it has no fields,
constructor, standalone object, or class vtable. NSMETA 1.28 publishes its public contract and
normative method-slot order; reordering those slots is an observable compatibility change for
precompiled defaults.
