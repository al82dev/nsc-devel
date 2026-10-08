# Tanda Universal 58: acceso nativo de StringView

## Alcance

Esta tanda elimina exclusivamente la obligación histórica de que los accesos fuera de rango
de vistas y spans sean recuperables mediante excepciones dentro de la fixture C23. La auditoría
general de `throws`, excepciones, RAII, `move` y `copy` queda fuera de esta tanda.

## Problema confirmado

Las 684 pruebas CTest pasaron, pero `cc1_c23_views_and_lifetimes` devolvió 8 porque la fixture
exigía que `StringView[index]` lanzara `ArgumentOutOfRangeException`. Esa política no es una
necesidad del compilador ni corresponde al contrato nativo de `std::string_view::operator[]`.

La implementación temporal tampoco era adecuada: convertía un índice inválido en el carácter
centinela `\0`, ocultando un error de precondición como si fuera un dato válido.

## Cambio

- `StringView[index]` realiza el acceso directo y no declara ni lanza excepciones.
- La precondición de rango pertenece al llamador, como en un acceso nativo.
- `UncheckedAt` se conserva para no romper consumidores existentes; su posible consolidación
  corresponde a la auditoría posterior.
- La fixture conserva las comprobaciones funcionales de vistas, spans, optional y lexer, pero
  deja de imponer que sus indexadores o accesores deban usar excepciones.
- No se modifica el compilador, `Span`, `Optional`, CMake, NSC Builder ni la infraestructura.

## Efecto

No hay costo de comprobación, construcción de excepción ni valor centinela en el indexador. La
complejidad permanece `O(1)` y el acceso inválido viola la precondición del API nativo.

## Validación dirigida

Compilar y ejecutar únicamente `cc1_c23_views_and_lifetimes` en O0 con NSLink. El resultado
esperado es 42. No se ejecuta baseline ni batería completa.
