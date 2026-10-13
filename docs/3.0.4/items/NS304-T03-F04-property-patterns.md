# NS304-T03-F04 — Completar property patterns anidados, posicionales, relacionales y lógicos

- **Estado**: CERRADO (26/147)
- **Rama**: development

## Propósito

Cerrar secuencialmente el ítem que habilita property patterns anidados, positional patterns por
deconstrucción (`Point(x, y)`), relacionales tipados y operadores lógicos con precedencia canónica.

## Contrato verificado

- **Parser** (`parsePatternPrimary`, `lowerPattern`, `emitPatternBindings`):
  - property patterns anidados;
  - positional `T(a, b, c)` con aridad exacta (`PatternKind::Positional` para tipo conocido,
    `PatternKind::Variant` para caso nominal);
  - precedencia `!` > `&` > `|` con agrupación explícita;
  - relacionales `< <= > >=` tipados sin conversión silenciosa.
- **Sema** (`bindDeconstructionSource`):
  - resolución de `Deconstruct` por símbolo (`memberCandidatesForType`), no por texto;
  - rechazo de aridad incorrecta (`deconstruction target has N element(s)...`);
  - rechazo de `Deconstruct` ambiguo, inaccesible o inexistente con span estructurado;
  - preservación de ownership (move-only/owning rechazado sin bindings `move` explícitos).
- Un pattern de lectura no mueve el objeto inspeccionado.

## Evidencia (O0)

| Caso | Resultado | Duración |
|---|---|---|
| `TL-LANG-311` (relacional/lógico/anidado) | PASS | 290,594 us |
| `TL-LANG-322` (posicional `Point(x, y)` vía `Deconstruct`) | PASS | 704,763 us |
| `TP-PAR-357` (parser posicional) | PASS | 288 us |
| `TL-LANG-308` (guard F01 list patterns) | PASS | 269,399 us |
| `TL-LANG-309` (guard F02 rest/slice) | PASS | 282,312 us |
| `TL-LANG-310` (guard F03 sustratos) | PASS | 551,340 us |

## Comando de validación

```powershell
$env:TMP=$env:TEMP="A:\nsc-devel\_tmp"
A:\nsc-devel\build-compiler-suite-v3\debug\nsc_component_tests.exe --case TL-LANG-311
# ... --case TL-LANG-322, TP-PAR-357, TL-LANG-308, TL-LANG-309, TL-LANG-310
```

## Riesgos / limitaciones pendientes

- Ninguna dentro del alcance de F04. La deconstrucción de tuplas con elementos owning/move-only
  sigue deliberadamente restringida a bindings `move` explícitos (F06).
