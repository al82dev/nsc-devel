# Type aliases and opaque aliases / Alias de tipo y alias opacos

## Español

NativeSharp 3 ofrece dos contratos distintos. Un alias transparente conserva la sintaxis
histórica `using Nombre = Tipo;`: el compilador sustituye el nombre por el tipo destino. No crea
identidad, no cambia el layout y no puede distinguir overloads.

```nativesharp
using Counter = int;
Counter value = 42; // Es exactamente int.
```

Un alias opaco se declara con `opaque type` y crea un tipo-valor nominal nuevo:

```nativesharp
public opaque type Meters = int;

Meters distance = Meters.FromRepresentation(42);
int raw = distance.ToRepresentation();
```

`FromRepresentation` y `ToRepresentation` son operaciones explícitas y `internal`. Sólo el módulo
que declara el alias puede verlas. No existe conversión implícita en ninguna dirección y el campo
físico no es accesible desde código fuente.

### Representación y ABI

El tipo opaco usa el layout ordinario de un `struct` nominal de un solo campo. No reserva heap, no
añade cabecera y no necesita soporte de runtime. Su tamaño y alineación son los de su representación
en Windows x64. La identidad, sin embargo, es la del nombre declarado y no la del tipo subyacente.

NSMETA transporta el layout privado para que un consumidor pueda pasar y devolver el valor con el
ABI correcto, pero no publica las operaciones internas de conversión. Un consumidor puede conservar,
copiar y entregar un valor obtenido de la API propietaria; no puede fabricarlo desde la
representación ni extraerla.

Usar el tipo opaco en una firma `export extern C` es una decisión FFI explícita: congela su layout
físico en ese límite. Si se desea mantener también opaca la representación frente a C, exponga en
su lugar un handle o funciones que reciban y devuelvan la representación elegida.

Cambiar la representación de un alias opaco público puede cambiar tamaño, alineación o clasificación
de llamada y por tanto es un cambio ABI. Los ciclos por valor, directos o indirectos, se rechazan
como layouts recursivos incompletos.

### Genéricos

Un tipo opaco puede cerrar un tipo genérico ordinario y también puede declarar parámetros propios:

```nativesharp
public opaque type Handle<T> = T*;
Box<Meters> measured;
```

Cada cierre conserva identidad nominal. Los constraints y límites de monomorfización se aplican por
las mismas rutas que a cualquier otro tipo-valor.

## English

NativeSharp 3 deliberately separates two contracts. `using Name = Type;` is transparent: the name
is substituted with its target and creates no new identity, layout, or overload distinction.

`opaque type Name = Representation;` creates a nominal value type. The declaring module receives
the explicit, internal `FromRepresentation` and `ToRepresentation` operations. Neither operation is
published through NSMETA, the physical field is private, and no implicit conversion exists in either
direction.

An opaque alias uses the ordinary one-field struct layout. It adds no heap allocation, object header,
or runtime wrapper; size and alignment match the representation on Windows x64 while type identity
remains nominal. Metadata consumers receive the private physical layout required for ABI-correct
passing and returning, but not the representation boundary operations.

Putting an opaque alias in an `export extern C` signature is an explicit FFI opt-in that freezes the
physical layout at that boundary. Changing a public opaque representation can therefore be an ABI
break. Direct and indirect by-value representation cycles are rejected deterministically. Opaque
aliases participate in generic construction and constraints as ordinary nominal value types.
