# NS304-T03-F05 — Usefulness/exhaustiveness para longitud, OR-pattern y dominios abiertos

- **Estado**: CERRADO (27/147)
- **Rama**: development

## Propósito

Extender el análisis de usefulness/exhaustiveness de `match` más allá de los dominios cerrados
(variantes/enums) para cubrir longitudes de list patterns, alternativas OR y dominios abiertos.

## Contrato verificado

- **Longitud** (`analyzeListMatch`, `SumTypeSemantics.cpp`): modela longitudes exactas y con rest
  (`+`); un conjunto finito de longitudes exactas sin rest nunca cubre el dominio infinito.
- **OR-pattern**: la cobertura compuesta expande alternativas (`|`) para casos y para longitudes;
  un brazo es redundante sólo cuando todas sus alternativas ya están cubiertas.
- **Dominios abiertos** (`int`/`string`/literal): sin wildcard se reporta
  `match over an open or unresolved domain requires a final wildcard or binding arm`.
- **Dominios cerrados** (`analyzeClosedMatch`): variantes/enums con metadata de casos; missing
  cases e unreachable arms con span.
- Puntos de reporte integrados en ambas vías (`ProjectMethodBodyModel.cpp` 4499/8865 para
  longitudes y 4549/8915 para cerrados).

## Evidencia (O0)

| Caso | Resultado | Duración |
|---|---|---|
| `TL-LANG-267` (modelo: unión de longitudes exactas, exacta+rest, redundancia OR) | PASS | 195 us |
| `TL-LANG-312` (Parser→Frontend→Sema: `[x]|[x,y]`, `[]|[_,..]`, redundancia, dominio abierto) | PASS | 480,087 us |
| `TL-LANG-311` (guard F04 relacional/lógico) | PASS | 352,987 us |
| `TL-LANG-322` (guard F04 posicional) | PASS | 745,261 us |
| `TP-PAR-357` (guard F04 parser posicional) | PASS | 328 us |
| `TL-LANG-308` (guard F01) | PASS | 291,312 us |
| `TL-LANG-309` (guard F02) | PASS | 331,666 us |
| `TL-LANG-310` (guard F03) | PASS | 661,300 us |

## Comando de validación

```powershell
$env:TMP=$env:TEMP="A:\nsc-devel\_tmp"
A:\nsc-devel\build-compiler-suite-v3\debug\nsc_component_tests.exe --case TL-LANG-312
# ... --case TL-LANG-267, TL-LANG-311, TL-LANG-322, TP-PAR-357, TL-LANG-308..310
```

## Riesgos / limitaciones pendientes

- Ninguna dentro del alcance de F05. La certificación de metadata sin fuentes y la paridad
  host/self-host se consolidan en F08.
