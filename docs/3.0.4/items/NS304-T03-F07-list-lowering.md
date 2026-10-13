# NS304-T03-F07 — Bajar a length checks, branches y borrows sin colección ni boxing

- **Estado**: CERRADO (29/147)
- **Rama**: development

## Propósito

Demostrar que el lowering de list patterns emite únicamente length checks, loads, comparaciones,
branches y borrows, sin colección, boxing, buffer temporal ni materialización de slice.

## Contrato verificado

- **Length check**: `makePatternCondition` (List) emite `Length >=/== fixedCount` (marcado
  `list-pattern-length`).
- **Element tests**: `source[index]` directo (prefijo) o `source[Length - fromEnd]` (sufijo), sin
  colección intermedia.
- **Rest capture**: `source.Slice(...)` (marcado `list-pattern-rest`) baja a `SpanFromArray` /
  `ReadOnlySpanFromArray` / `StringViewFromString` — una vista prestada SIN copia (F03).
- **Oráculo MIR** (`TL-LANG-366`): el cuerpo de `Read(int[] values)` con `[10, .. var middle, 40]`
  no contiene ningún nodo de asignación (`ArrayAllocate`, `StackAllocation`, `RawAllocation`,
  `RangeArrayAllocate`, `AllocateAndConstruct`) y sí contiene el borrow `SpanFromArray`/
  `ReadOnlySpanFromArray`.

## Evidencia (O0)

| Caso | Resultado | Duración |
|---|---|---|
| `TL-LANG-366` (oráculo MIR noheap: rest = borrow, 0 asignaciones) | PASS | 1,187,473 us |
| `TL-LANG-364` (guard alias observable + rechazo readonly) | PASS | 864,741 us |
| `TL-LANG-310` (guard sustrato string) | PASS | 569,666 us |
| `TL-LANG-357..362` (sustratos array/inline/span/readonlyspan/stringview/protocolo) | PASS | — |
| `TL-LANG-363` (UTF-8 no ASCII) | PASS | — |
| `TL-LANG-365` (escape rest view stackalloc) | PASS | — |
| `TL-LANG-313` (guard F06) | PASS | 304,909 us |

## Comando de validación

```powershell
$env:TMP=$env:TEMP="A:\nsc-devel\_tmp"
A:\nsc-devel\build-compiler-suite-v3\debug\nsc_component_tests.exe --case TL-LANG-366
```

## Riesgos / limitaciones pendientes

- El oráculo MIR cubre el sustrato array (el rest capture es un borrow por construcción para todos
  los sustratos). La observación LLVM completa se consolida en F08.
