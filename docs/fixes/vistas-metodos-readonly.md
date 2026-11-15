# Miembros readonly de StringView, Span y ReadOnlySpan

Desde `8dc2a1720` (2026-11-09) y `d00b3a869` (2026-11-12), un método que no es `readonly` cuenta
como potencialmente mutador y no se puede llamar sobre un valor mientras está viva una vista
obtenida de ese valor. `StringView`, `Span<T>` y `ReadOnlySpan<T>` solo declaraban `readonly`
`Length`, `IsEmpty` (en parte) y el indexador de lectura. El patrón ordinario "tomar un recorte y
seguir usando el original" se rechazaba: en la fixture cc1 c23, `trimmed.Slice(5)` con `first` vivo
dio "cannot call potentially mutating method while a borrowed field view is alive".

La regla es correcta; lo que faltaba era la declaración. Se marcan `readonly` los métodos que no
asignan campos:

- `StringView`: `Data`, `UncheckedAt`, `Slice` (dos), `IndexOf`, `LastIndexOf`, `Contains`,
  `StartsWith`, `EndsWith`, `TrimStart`, `TrimEnd`, `Trim`, `ToString`.
- `Span<T>`: `IsEmpty`, `Data`, `UncheckedAt`, `Slice` (dos), `CopyTo`, `CopyToOverlapping`,
  `AsReadOnly`.
- `ReadOnlySpan<T>`: `IsEmpty`, `Data`, `UncheckedAt`, `Slice` (dos), `CopyTo`.

Los métodos que escriben a través de la vista (`Fill`, `Put`, `RefAt`, `UncheckedSet`, el indexador
de `Span<T>`) no cambian. El compilador verifica `readonly`: una asignación de campo dentro de un
método `readonly` da "left side of assignment is not assignable", así que la marca es una propiedad
comprobada y no una etiqueta.

## Prueba

`tests/integration/view_readonly_members.py` (`nsc.compiler.view-readonly-members`):

- El programa válido usa cada método en ese patrón y comprueba valores en O0 y O2; con el SDK
  anterior no compila.
- Un tipo de vista propio con un método no `readonly` sigue rechazado en el mismo patrón, con el
  mensaje exacto.
- Un método `readonly` que asigna un campo no compila.
