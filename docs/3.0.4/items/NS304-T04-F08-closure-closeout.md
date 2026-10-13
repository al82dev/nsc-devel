# NS304-T04-F08 — Layout, drop, escape, PDB y closures con errors

- **Estado**: CERRADO (38/147)
- **Rama**: development

## Propósito

Cerrar la tanda T04 observando el layout determinista, el drop inverso de capturas owned, el
rechazo de escape y la ejecución de closures combinadas con errores tipados.

## Contrato verificado

- **Layout**: `computeClosureLayout` + `closureLayoutSize/Alignment` en MIR (F08 auditoría).
- **Drop inverso una vez**: el helper `__nsc_closure_context_destroy_` destruye las capturas owned;
  `TL-LANG-368` observa el destructor exactamente una vez (`Drops=1`).
- **Escape**: `ProjectReferenceEscapeEvaluator` rechaza una closure que escapa un local capturado
  (`TL-LANG-368` negativo).
- **Throws/cleanup**: `TL-LANG-321` ejecuta closure sobre delegate `throws Exception` con try/catch.
- **PDB**: la emisión CodeView se produce con `-g` (helper de destroy con subprograma debug); la
  inspección de contenido PDB se consolida en T16 (debug/self-host).

## Evidencia (O0)

| Caso | Resultado | Duración |
|---|---|---|
| `TL-LANG-368` (drop owned una vez + escape rechazado) | PASS | 332,066 us |
| `TL-LANG-320` (guard closure exec) | PASS | 337,000 us |
| `TL-LANG-321` (guard closure + throws) | PASS | — |
| `TL-LANG-367` (guard F07 ranking) | PASS | 373,863 us |

## Comando de validación

```powershell
$env:TMP=$env:TEMP="A:\nsc-devel\_tmp"
A:\nsc-devel\build-compiler-suite-v3\debug\nsc_component_tests.exe --case TL-LANG-368
```

## Riesgos / limitaciones pendientes

- La inspección puntual del contenido PDB (lambda + captures + helper) se consolida en T16
  (ABI/metadata/debug/self-host), que es la tanda canónica de depuración.
