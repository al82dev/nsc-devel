# 324 — Structs anónimos / Anonymous structs

## Resultado / Outcome

NativeSharp acepta un struct inline sin nombre fuente cuando la declaración conserva un campo
contenedor explícito:

```nativesharp
public struct Packet
{
    public byte Tag;
    public struct
    {
        public short Low;
        public int High;
    } Payload;
}
```

El acceso permanece calificado como `packet.Payload.High`. Los miembros no aparecen directamente
en `Packet`: esa promoción pertenece al punto 326.

NativeSharp accepts a source-anonymous inline struct when the declaration retains an explicit
carrier field. Access remains qualified through that field; member promotion is deliberately left
to item 326.

## Representación / Representation

El parser conserva las dos identidades necesarias sin crear una categoría paralela aguas abajo:

- un `StructDeclaration` anidado con nombre interno determinista
  `__nsc_anonymous_struct_f<file-id>_<source-offset>`;
- sus fields reales en `children()`;
- el `FieldDeclaration` contenedor en `first()`, fuera de esos hijos.

Así, `ProjectNameLookup` registra el layout por la ruta ya existente de tipos anidados.
`ProjectMemberModel` entrega los fields del cuerpo sólo al tipo sintetizado y el field contenedor
sólo al agregado exterior. Desde ese punto, claves canónicas, layout, acceso de miembros,
HIR/MIR y LLVM reutilizan exactamente el soporte de structs nominales.

The parser retains a deterministic synthesized nested type and a separate ordinary carrier field.
Name lookup, member collection, canonical layout and code generation then reuse the existing named
struct pipeline instead of introducing an anonymous runtime type.

## Layout y límites / Layout and boundaries

- El valor anónimo se almacena inline, sin descriptor ni asignación.
- `pack(N)`, `align(N)` y `size(N)` antes de `struct` pertenecen al cuerpo anónimo.
- `at(N)` pertenece al field contenedor.
- Los cuerpos pueden anidarse recursivamente y conservan identidades distintas por archivo y offset fuente.
- El nombre del field después de `}` es obligatorio en el punto 324.
- `struct { ... };` recibe un diagnóstico que señala la promoción futura del punto 326.
- Los unions anónimos pertenecen al punto 325.
- No cambian MIR 1.6, NSMETA 1.16, Object ABI 2, Runtime ABI, runtime, bindings ni tutoriales.

## Evidencia / Evidence

`Item324AnonymousStructTests.cpp` verifica en O0:

- identidad interna determinista y separación correcta de miembros;
- field contenedor con tipo canónico sintetizado;
- layout natural del cuerpo (`size=8`, `align=4`) y del contenedor (`size=16`);
- offset, tamaño y alineación físicos del field contenedor;
- `pack(1)` sobre el cuerpo anónimo y `at(4)` sobre el field contenedor;
- anidamiento recursivo;
- ausencia de colisión entre offsets iguales de archivos parciales distintos;
- lowering LLVM mediante offsets ordinarios;
- enlace y ejecución con resultado 42;
- rechazo explícito de una declaración sin nombre.

Las regresiones adyacentes cubren parser, name lookup, member model, layout de structs, lowering OOP
y los puntos 317–323.

## Mensaje de commit / Commit message

`Añade structs anónimos con campo contenedor / Add anonymous structs with carrier fields`
