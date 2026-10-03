# Bloques `impl` y coherencia / `impl` blocks and coherence

## Español

NativeSharp 3 permite separar la declaración física de una clase de la implementación de un
trait sin crear un tipo, objeto ni tabla adicional:

```nativesharp
trait IScore
{
    int Score();
    int Bonus() { return Score() + 2; }
}

class Number
{
    private int value;
    public Number(int source) { value = source; }
}

impl IScore for Number
{
    int Score() { return value; }
}
```

Los miembros del bloque son witnesses públicos del contrato, incluso cuando no escriben
`public`. No pueden ser privados/protegidos ni introducir campos, constructores, destructores o
tipos anidados. Propiedades estáticas, indexers y operadores siguen las mismas reglas del trait y
del tipo concreto.

### Regla de coherencia sellada

El módulo que escribe el `impl` debe ser dueño del tipo concreto. Puede implementar un trait local
o importado para una clase local, pero no puede añadir una implementación a una clase importada.
Ésta es la alternativa sellada de NativeSharp a la orphan rule:

- cada par `Trait, Type` tiene a lo sumo un `impl` explícito;
- una conformidad escrita en la lista base y un bloque `impl` para el mismo par son duplicados;
- un bloque `impl` sólo acepta un `trait`, no una interfaz ordinaria;
- la clase objetivo debe ser no estática y vivir en el mismo módulo y namespace;
- la metadata de la clase propietaria es la autoridad global para builds separados.

Por ello agregar o retirar un `impl` público cambia la superficie/compatibilidad de la biblioteca:
puede cambiar constraints satisfechos, conversiones a `dyn` y tablas de despacho. El consumidor
no puede superponer otra respuesta.

### `impl` condicional

Una plantilla puede extenderse con la forma:

```nativesharp
class Box<T> where T : copy { }
impl<T> IScore<T> for Box<T> where T : copy { /* witnesses */ }
```

Los parámetros, argumentos del objetivo y cláusulas `where` deben repetir exactamente los de la
clase. Esta regla conservadora evita que una especialización obtenga dos respuestas dependiendo
del orden de módulos. El bloque se fusiona antes de monomorfizar y se publica junto con la fuente
reutilizable de la plantilla en NSMETA; un consumidor separado obtiene la misma implementación.

## English

NativeSharp 3 can separate a class body from a trait implementation without creating another
type, object, or dispatch table. Members in an `impl` block are public contract witnesses even
when `public` is omitted. They cannot be private/protected or introduce fields, constructors,
destructors, or nested types.

NativeSharp uses sealed coherence: the module writing an `impl` must own the concrete target type.
It may implement a local or imported trait for a local class, but it cannot attach behavior to an
imported class. Each `Trait, Type` pair has at most one explicit impl; base-list conformance plus an
impl is also a duplicate, and ordinary interfaces are not valid impl contracts.

A conditional generic impl must repeat the owner's generic parameter list, target arguments, and
`where` clauses exactly. It is merged before monomorphization. NSMETA transports its original
source extension with a reusable public template, so separately compiled consumers reach the same
coherent result. Adding or removing a public impl is therefore a library compatibility change.
