# NS304-T03-F08 — Diagnósticos de casos faltantes, redundancia, escape y move inválido

- **Estado**: CERRADO (30/147)
- **Rama**: development

## Propósito

Cerrar la tanda T03 con los diagnósticos de casos faltantes, redundancia, bindings incompatibles,
escape de rest view y move inválido, y ejecutar el consolidado T03 completo.

## Contrato verificado

- **Faltantes (variante)**: `analyzeClosedMatch` + "missing variant case".
- **Faltantes (longitud)**: `analyzeListMatch` + "missing list length".
- **Redundancia (variante)**: `analyzeClosedMatch.unreachableArms` → "unreachable for the closed
  variant domain".
- **Redundancia (list)**: `analyzeListMatch.unreachableArms` → "unreachable for the list-length
  domain" (nueva prueba `TL-LANG-315`).
- **Bindings incompatibles**: `orPatternBindingsCompatible` (nombre+tipo+modo).
- **Escape de rest view**: `TL-LANG-365` (negativo exacto) + `TL-LANG-309` (guard local).
- **Move inválido / uso después de move**: maquinaria de ownership (`movedRaiiLocals_`,
  `movedOwnedParameters_`) con diagnósticos "a moved value cannot be borrowed", "cannot partially
  move", "move-only value ... use move(source)".
- **Property/positional inválidos**: `bindDeconstructionSource` (aridad/ambiguo/inaccesible).

## Evidencia (O0)

| Caso | Resultado | Duración |
|---|---|---|
| `TL-LANG-315` (brazo de list redundante = unreachable) | PASS | 229,915 us |
| Consolidado T03 (20 casos: F01-F08 + guards) | 20/20 PASS | — |

## Consolidado T03

`TL-LANG-308`, `309`, `310`, `357..363`, `311`, `322`, `TP-PAR-357`, `267`, `312`, `313`, `364`,
`366`, `315` → **20/20 PASS** (0 fallos).

## Comando de validación

```powershell
$env:TMP=$env:TEMP="A:\nsc-devel\_tmp"
A:\nsc-devel\build-compiler-suite-v3\debug\nsc_component_tests.exe --case TL-LANG-315
```

## Riesgos / limitaciones pendientes

- La observación LLVM completa (F07 #5) se cubre con el oráculo MIR `TL-LANG-366`; no se comparan
  hashes ni el número total de instrucciones.
