# NS304-T04-F07 — Ranking de method groups, overload priority y conversiones a fn*/fn&

- **Estado**: CERRADO (37/147)
- **Rama**: development

## Propósito

Congelar el ranking de method groups (prioridad por coincidencia exacta sobre el resto), las
conversiones a `fn*`/`fn&` y el rechazo de captura para puntero crudo.

## Contrato verificado

- **Ranking por coincidencia exacta**: `fn*(int) -> int f = Pick` con `Pick(int)`/`Pick(long)` elige
  el overload `int` (coincidencia exacta); `fn&(int) -> int r = Pick` convierte el mismo grupo.
- **Ambigüedad**: `var f = Pick` con multi-overload reporta `multiple overloads cannot infer a
  natural callable type` (5949-5960).
- **Rechazo de captura para `fn*`**: una lambda capturante no convierte a `fn*` (sin environment).
- Conversión por símbolo, no por orden de archivo (`methodCandidates` centralizado).

## Evidencia (O0)

| Caso | Resultado | Duración |
|---|---|---|
| `TL-LANG-367` (fn*/fn& ranking + captura rechazada + ambigüedad) | PASS | 373,863 us |
| `TL-LANG-319` (guard method group natural único) | PASS | 308,078 us |
| `TL-LANG-314` (guard lambda natural) | PASS | 309,001 us |
| `TL-LANG-320` (guard closure exec) | PASS | 337,000 us |

## Comando de validación

```powershell
$env:TMP=$env:TEMP="A:\nsc-devel\_tmp"
A:\nsc-devel\build-compiler-suite-v3\debug\nsc_component_tests.exe --case TL-LANG-367
```

## Riesgos / limitaciones pendientes

- El desempate por conversión estándar (no exacta) entre candidatos se materializa con la ruta de
  overload de llamada; la matriz puntual de F07 congela prioridad exacta + ambigüedad + `fn*`/`fn&`.
