# Campos y parámetros promovidos / Promoted fields and parameters

## Español

NativeSharp 3.0 permite declarar el estado y el constructor primario de un `struct` o
`record struct` en una sola lista:

```nativesharp
public struct Point(public int X, public readonly int Y = 0)
{
}
```

La declaración anterior se desazucara antes del enlace de nombres a dos campos ordinarios y un
constructor ordinario equivalente a:

```nativesharp
public struct Point
{
    public int X;
    public readonly int Y;

    public Point(int X, int Y = 0)
    {
        this.X = X;
        this.Y = Y;
    }
}
```

El orden textual de los parámetros determina el orden de evaluación de argumentos, asignación de
campos y declaración física. El layout final sigue las reglas Windows x64 normales, incluida la
alineación y el padding; la sintaxis no introduce un objeto, captura, cabecera ni ABI alterna.

Cada parámetro puede declarar `public`, `private` o `internal`, y puede llevar `readonly`. Si se
omite la accesibilidad, un `record struct` promueve un campo público y un `struct` ordinario uno
privado. `protected` es inválido porque los structs no son heredables. El constructor sintetizado
es público.

Los valores predeterminados pertenecen al parámetro del constructor, deben cumplir las reglas
ordinarias de constantes y sólo pueden preceder a otros parámetros predeterminados. Los nombres
duplicados o que colisionan con miembros escritos en el cuerpo se rechazan con la ubicación de la
declaración previa.

Los nombres del tipo, campos y parámetros llegan a CodeView. Los records y structs genéricos
conservan el mismo desazucarado al especializarse. Una lista vacía `struct Empty()` crea un
constructor público sin parámetros.

En este corte no se permiten `params`, `ref`, `out`, `in`, `owned` ni `borrowed` en la lista
primaria. Esas formas necesitan los contratos de movimiento y destrucción parcial de T13/T14;
aceptarlas antes produciría captura u ownership implícitos. Las clases y la herencia tampoco forman
parte de esta sintaxis: F05 define constructores primarios exclusivamente para valores `struct`.

## English

NativeSharp 3.0 can declare a `struct` or `record struct` state and primary constructor in one
list:

```nativesharp
public struct Point(public int X, public readonly int Y = 0)
{
}
```

Before name binding, that declaration is desugared into two ordinary fields and one ordinary
constructor equivalent to:

```nativesharp
public struct Point
{
    public int X;
    public readonly int Y;

    public Point(int X, int Y = 0)
    {
        this.X = X;
        this.Y = Y;
    }
}
```

Textual parameter order controls argument evaluation, field assignment, and physical declaration
order. Final layout follows the regular Windows x64 alignment and padding rules; the syntax adds no
object, capture, header, or alternate ABI.

Each parameter may specify `public`, `private`, or `internal`, and may be `readonly`. When access is
omitted, a `record struct` promotes a public field while a plain `struct` promotes a private one.
`protected` is invalid because structs are not inheritable. The synthesized constructor is public.

Default values belong to the constructor parameter, follow the ordinary constant rules, and may
only be followed by other defaulted parameters. Duplicate names and collisions with body members
are rejected while preserving the previous declaration location.

Type, field, and parameter names reach CodeView. Generic records and structs retain the same
desugaring after specialization. An empty list, `struct Empty()`, creates a public parameterless
constructor.

This cut rejects `params`, `ref`, `out`, `in`, `owned`, and `borrowed` primary parameters. Those
forms require T13/T14 move and partial-destruction contracts; accepting them earlier would create
implicit capture or ownership. Classes and inheritance are not part of this syntax either: F05
defines primary constructors exclusively for `struct` values.
