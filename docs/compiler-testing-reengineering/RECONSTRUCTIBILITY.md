# Estado de reconstructibilidad

Todos los componentes comienzan como `NO VERIFICADO` y sólo pueden llegar a `RECONSTRUIBLE` después de
responder las veinte preguntas aplicables, cerrar su matriz de contratos y superar una segunda auditoría
independiente.

El estado canónico por componente se conserva en `inventory/components.csv`. Este documento no sustituye esa
tabla; registra el criterio y enlazará los informes individuales conforme se produzcan.

## Componentes auditados

| Componente | Resultado | Símbolos | Contratos | Casos diseñados | Evidencia |
|---|---|---:|---:|---:|---|
| `Host.Basic` | `RECONSTRUIBLE` | 60 | 16 | 88 | `audits/TANDA-03-HOST-BASIC-AUDIT.md` |
| `Host.Source` | `RECONSTRUIBLE` | 23 | 8 | 58 | `audits/TANDA-03-HOST-SOURCE-AUDIT.md` |
| `Host.Diagnostics` | `RECONSTRUIBLE` | 43 | 12 | 75 | `audits/TANDA-03-HOST-DIAGNOSTICS-AUDIT.md` |
| `Host.Lexer` | `RECONSTRUIBLE` | 51 | 14 | 110 | `audits/TANDA-04-HOST-LEXER-AUDIT.md` |
| `Host.Parser` | `RECONSTRUIBLE` | 174 | 42 | 536 | `audits/TANDA-04-HOST-PARSER-AUDIT.md` |
| `Host.AST` | `RECONSTRUIBLE` | 155 | 20 | 230 | `audits/TANDA-04-HOST-AST-AUDIT.md` |
| `Host.Language` | `RECONSTRUIBLE` | 546 | 21 | 286 | `audits/TANDA-05-HOST-LANGUAGE-AUDIT.md` |
| `Host.Sema` | `RECONSTRUIBLE` | 214 | 30 | 321 | `audits/TANDA-05-HOST-SEMA-AUDIT.md` |
| `Host.Frontend` | `RECONSTRUIBLE` | 1,392 | 63 | 504 | `audits/TANDA-06-HOST-FRONTEND-AUDIT.md` |
| `Host.HIR` | `RECONSTRUIBLE` | 88 | 29 | 232 | `audits/TANDA-07-HOST-HIR-AUDIT.md` |
| `Host.MIR` | `RECONSTRUIBLE` | 152 | 38 | 304 | `audits/TANDA-07-HOST-MIR-AUDIT.md` |
| `Host.CFG` | `RECONSTRUIBLE` | 77 | 26 | 208 | `audits/TANDA-07-HOST-CFG-AUDIT.md` |
| `Host.SSA` | `RECONSTRUIBLE` | 102 | 42 | 336 | `audits/TANDA-07-HOST-SSA-AUDIT.md` |
| `Host.Dominance` | `RECONSTRUIBLE` | 81 | 32 | 256 | `audits/TANDA-07-HOST-DOMINANCE-AUDIT.md` |
| `Host.Passes` | `RECONSTRUIBLE` | 158 | 56 | 448 | `audits/TANDA-07-HOST-PASSES-AUDIT.md` |

`RECONSTRUIBLE` describe suficiencia documental para la revisión congelada. No significa que la suite nueva
esté implementada ni que se haya ejecutado validación.
