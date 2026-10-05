# NS3-T13-F01 — Atributos de representación / Representation attributes

## Español

NativeSharp 3.0 incorpora `repr(native)`, `repr(C)` y `repr(transparent)` como contratos de
representación productivos para `struct` y `union` en Windows x64.

- `repr(native)` selecciona el layout nativo de esta ABI mayor. Puede evolucionar en otra ABI
  mayor y no debe publicarse como contrato C estable.
- `repr(C)` fija el orden declarado de campos y usa las reglas Win64/MSVC ya compartidas por el
  generador de headers, NSMETA y el backend. Puede combinarse con `pack(N)`, `align(N)`, `size(N)`
  y offsets explícitos.
- `repr(transparent)` exige exactamente un campo de instancia con tamaño distinto de cero. El
  wrapper conserva el carrier y ABI de ese campo; por ello no puede ser `union` ni alterar
  `pack`, `align` o `size`.

El parser conserva la representación en el AST y el modelo de layout la valida antes de producir
MIR, LLVM, metadata o headers. Atributos duplicados, valores desconocidos y aplicaciones a clases
se rechazan con diagnóstico determinista. La prueba dirigida realiza una ejecución O0 enlazada por
NSLink y cubre las combinaciones inválidas sin ejecutar baseline.

## English

NativeSharp 3.0 provides `repr(native)`, `repr(C)`, and `repr(transparent)` as productive
representation contracts for Windows x64 `struct` and `union` declarations.

- `repr(native)` selects this ABI major's native layout. It may evolve in a later ABI major and
  must not be published as a stable C contract.
- `repr(C)` fixes declaration order and uses the Win64/MSVC rules already shared by C-header
  generation, NSMETA, and the backend. It may be combined with `pack(N)`, `align(N)`, `size(N)`,
  and explicit field offsets.
- `repr(transparent)` requires exactly one non-zero-sized instance field. The wrapper preserves
  that field's carrier and ABI, so it cannot be a union or override packing, alignment, or size.

The parser retains the representation in the AST and the layout model validates it before MIR,
LLVM, metadata, or headers are produced. Duplicate attributes, unknown values, and applications
to reference classes receive deterministic diagnostics. The directed test executes an O0 program
linked by NSLink and covers invalid combinations without running a baseline.
