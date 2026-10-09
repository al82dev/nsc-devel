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

`RECONSTRUIBLE` describe suficiencia documental para la revisión congelada. No significa que la suite nueva
esté implementada ni que se haya ejecutado validación.
