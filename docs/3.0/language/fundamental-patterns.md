# Fundamental patterns / Patterns fundamentales

## Español

### Gramática productiva

Los patterns de un `match` de edición 3 se combinan recursivamente:

```text
pattern := _
         | literal
         | var nombre | ref nombre | move nombre
         | (pattern, pattern, ...)
         | { Campo: pattern, ... }
         | Tipo { Campo: pattern, ... }
         | Caso | Caso(pattern)
         | entero..entero
         | pattern | pattern
```

- `_` descarta cualquier valor.
- Los literales admitidos son enteros, caracteres, strings, `true`, `false` y `null`.
- `a..b` es un rango entero semiabierto: incluye `a` y excluye `b`.
- Un tuple pattern exige la misma aridad que el tuple inspeccionado.
- Un struct pattern nombra campos y puede omitir los que no necesita observar.
- Un variant pattern comprueba primero el tag y sólo después extrae su payload.
- `|` conserva short-circuit. Sus alternativas pueden introducir bindings cuando todas publican
  exactamente los mismos nombres y modos (`var`, `ref` o `move`). El compilador distribuye las
  alternativas en brazos privados y fusiona sólo el camino seleccionado.

La forma histórica `Caso(nombre)` sigue siendo un binding por valor para no romper los programas
de variants existentes. En código nuevo se recomienda `Caso(var nombre)` porque hace visible la
intención y evita ambigüedad con un caso unitario anidado, que se escribe `Caso(Unidad())`.

### Modos de binding y ownership

```nativesharp
match (value)
{
    Some(var copy) => { Use(copy); },
    _ => { }
}
```

- `var nombre` produce un binding por valor inmutable dentro del brazo.
- `ref nombre` conserva alias mutable sólo cuando la ruta completa denota almacenamiento real:
  el valor gobernante o campos de un agregado gobernado por referencia. Los payloads variant y la
  deconstrucción tuple son proyecciones por valor; cruzarlas con `ref` se rechaza en vez de fabricar
  un alias falso.
- `move nombre` hace explícita la intención y produce un binding mutable. Para valores triviales
  la operación física coincide con una copia. Una extracción parcial que atravesaría una
  proyección por valor se rechaza; mover un agregado completo conserva el modelo ordinario de
  ownership y drop flags.

La expresión gobernante se evalúa exactamente una vez. La bajada usa un temporal y decisiones
guardadas; no introduce un loop sintético, de modo que `break`, `continue` y `return` dentro de un
brazo conservan su destino original.

### Diagnósticos deterministas

- Literales float se rechazan: NaN y cero con signo no forman un dominio cerrado de igualdad.
- Rangos vacíos/invertidos y rangos enteros solapados se diagnostican antes del lowering.
- Alternativas OR con nombres o modos de binding distintos se rechazan como incompatibles.
- Un brazo posterior a `_` o a un binding total es inalcanzable.
- Un dominio abierto exige un brazo final total (`_` o binding). Un variant cerrado puede omitirlo
  cuando todos sus casos quedan cubiertos; esa comprobación pertenece a T03-F03.

## English

### Productive grammar

Edition 3 `match` patterns compose recursively using the grammar shown above. `_` discards a
value; literal patterns accept integers, characters, strings, booleans, and null; `a..b` is a
half-open integer range. Tuple patterns require matching arity. Struct patterns name only the
fields they inspect. Variant patterns test the tag before touching the payload. OR patterns retain
short-circuit evaluation and may bind when every alternative exports identical names and modes;
only the selected alternative initializes the merged bindings.

The legacy `Case(name)` spelling remains a by-value binding for source compatibility. New code
should prefer `Case(var name)`. A nested unit case is unambiguous as `Case(Unit())`.

### Binding modes and ownership

- `var name` creates an immutable by-value arm binding.
- `ref name` preserves mutable aliasing only across a fully address-preserving path: the governing
  storage itself or fields reached from governing reference storage. Tuple deconstruction and
  variant payload extraction are value projections, so crossing either with `ref` is rejected
  rather than manufacturing a false alias.
- `move name` makes intent explicit and creates a mutable binding. A trivial value has the same
  physical operation as a copy. A partial owner extraction through a value projection is rejected;
  whole-value moves use the ordinary ownership/drop-flag model.

The governing expression is evaluated exactly once. Lowering uses a temporary and guarded
decisions, not a synthetic loop, so an arm's `break`, `continue`, and `return` retain their original
targets.

### Deterministic diagnostics

Float literals are rejected because NaN and signed zero do not define a closed equality domain.
Empty/reversed integer ranges and overlaps are diagnosed before lowering. OR alternatives must
bind identical names and modes. An arm following a total wildcard/binding arm is unreachable. An
open domain requires a final total arm; a closed variant may omit it when T03-F03 proves every case
covered.
