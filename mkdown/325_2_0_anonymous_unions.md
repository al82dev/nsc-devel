# 325 — Unions anónimos / Anonymous unions

## Resultado / Outcome

NativeSharp acepta una union inline sin nombre fuente cuando la declaración conserva un campo
contenedor explícito:

```nativesharp
public struct Packet
{
    public union
    {
        public int AsInt;
        public float AsFloat;
        public byte[4] Bytes;
    } Payload;
}
```

Las alternativas se superponen y el acceso permanece calificado, por ejemplo
`packet.Payload.AsInt`. Ningún miembro aparece directamente en `Packet`: esa promoción pertenece
al punto 326.

NativeSharp accepts a source-anonymous inline union when the declaration retains an explicit
carrier field. Alternatives overlap at offset zero and remain qualified through that field;
anonymous-member promotion is deliberately left to item 326.

## Representación / Representation

El parser reutiliza el modelo establecido por el punto 324:

- un `StructDeclaration` anidado con identidad interna determinista
  `__nsc_anonymous_union_f<file-id>_<source-offset>`;
- la marca `union` que ya consume el modelo de layout;
- las alternativas reales en `children()`;
- el `FieldDeclaration` contenedor en `first()`, fuera de esos hijos.

Name lookup y member collection siguen la ruta común de tipos anidados. Sólo
`ProjectObjectLayoutModel` interpreta la marca `union`; claves canónicas, acceso de miembros,
HIR/MIR y LLVM reutilizan el soporte ordinario de valores anidados.

The parser retains a deterministic synthesized nested value type, the existing union-layout marker
and a separate ordinary carrier field. All later stages except physical layout use the same path as
named structs and unions.

## Layout y límites / Layout and boundaries

- Todas las alternativas comienzan en offset cero.
- Tamaño y alineación cubren la alternativa mayor.
- No existe discriminante oculto ni seguimiento de la alternativa activa.
- `pack(N)`, `align(N)` y `size(N)` antes de `union` pertenecen al cuerpo anónimo.
- `at(N)` pertenece al field contenedor.
- Los cuerpos pueden anidarse con structs o unions anónimos.
- El nombre del field después de `}` es obligatorio en el punto 325.
- `union { ... };` diagnostica que la promoción pertenece al punto 326.
- No cambian MIR 1.6, NSMETA 1.16, Object ABI 2, Runtime ABI, runtime, bindings ni tutoriales.

## Evidencia / Evidence

`Item325AnonymousUnionTests.cpp` verifica en O0:

- identidad interna determinista y separación correcta de miembros;
- field contenedor con tipo canónico sintetizado;
- alternativas `int`, `float` y `byte[4]` superpuestas en offset cero;
- layout natural del cuerpo (`size=4`, `align=4`) y del contenedor (`size=12`);
- `pack(1)` sobre la union y `at(8)` sobre el field contenedor;
- anidamiento dentro de un struct anónimo;
- ausencia de colisión entre offsets iguales de archivos parciales distintos;
- lowering LLVM mediante offsets ordinarios;
- enlace y ejecución de lectura solapada con resultado 42;
- rechazo explícito de una declaración sin nombre.

Las regresiones adyacentes cubren name lookup, member model, layout, lowering OOP, unions nombradas
y los puntos 317–324.

## Mensaje de commit / Commit message

`Añade unions anónimos con campo contenedor / Add anonymous unions with carrier fields`
