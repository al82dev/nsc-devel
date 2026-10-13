# NS304-T03-F06 — Bindings `copy`, `move`, `ref` y `ref mut` en patterns

- **Estado**: CERRADO (28/147)
- **Rama**: development

## Propósito

Definir y transportar los cuatro modos de binding de pattern (`copy`, `move`, `ref`, `ref mut`)
desde el parser hasta LLVM, preservando la semántica de préstamo y rechazando escritura readonly.

## Contrato verificado

- **Enum único** `PatternBindingMode` (`SumTypeSemantics.h`): `Value`, `Reference`, `Move`,
  `Copy`, `ReferenceMutable` compartido entre parser y `SumTypeSemantics`.
- **Parser** (`parsePatternPrimary` 11025/11033): `copy x` → `Copy`; `ref mut x` → `ReferenceMutable`;
  `ref x` → `Reference`; `move x` → `Move`; `bindingModeName` los nombra.
- **OR-compatibilidad** (`orPatternBindingsCompatible`): alternativas OR deben coincidir en
  nombres, tipos y modos (incluido `ReferenceMutable`, 10481/12288/12437).
- **Sema/HIR/MIR/LLVM**: marcadores `local:pattern-ref` y `local:pattern-ref-mut`;
  `MethodLocalSymbol::referenceMutable` propaga la capacidad; `MatchValueExpression` es
  almacenamiento direccionable (sin copia materializada antes de `ref mut`).
- `ref` readonly rechaza escritura; `ref mut` conserva el alias mutable exclusivo.

## Evidencia (O0)

| Caso | Resultado | Duración |
|---|---|---|
| `TL-LANG-313` (copy/move/ref/ref mut + rechazo de escritura `ref`) | PASS | 304,909 us |
| `TL-LANG-364` (ownership: alias/no-copia observable, readonly) | PASS | 864,741 us |
| `TL-LANG-312` (guard F05) | PASS | 427,416 us |
| `TL-LANG-267` (guard F05) | PASS | 136 us |
| `TL-LANG-311` (guard F04) | PASS | 301,398 us |
| `TL-LANG-322` (guard F04) | PASS | 652,700 us |
| `TP-PAR-357` (guard F04) | PASS | 290 us |
| `TL-LANG-308/309/310` (guards F01-F03) | PASS | 289/275/569 ms |

## Comando de validación

```powershell
$env:TMP=$env:TEMP="A:\nsc-devel\_tmp"
A:\nsc-devel\build-compiler-suite-v3\debug\nsc_component_tests.exe --case TL-LANG-313
# ... --case TL-LANG-364, TL-LANG-312, TL-LANG-267, TL-LANG-311, TL-LANG-322, TP-PAR-357, TL-LANG-308..310
```

## Riesgos / limitaciones pendientes

- Ninguna dentro del alcance de F06. La certificación de metadata y paridad host/self-host se
  consolida en F08.
