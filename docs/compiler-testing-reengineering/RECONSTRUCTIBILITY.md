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
| `Host.Target` | `RECONSTRUIBLE` | 46 | 30 | 240 | `audits/TANDA-08-HOST-TARGET-AUDIT.md` |
| `Host.LLVM` | `RECONSTRUIBLE` | 802 | 64 | 512 | `audits/TANDA-08-HOST-LLVM-AUDIT.md` |
| `Host.Interop` | `RECONSTRUIBLE` | 178 | 48 | 384 | `audits/TANDA-09-HOST-INTEROP-AUDIT.md` |
| `Host.Linker` | `RECONSTRUIBLE` | 132 | 40 | 320 | `audits/TANDA-09-HOST-LINKER-AUDIT.md` |
| `Host.Runtime` | `RECONSTRUIBLE` | 129 | 48 | 384 | `audits/TANDA-09-HOST-RUNTIME-AUDIT.md` |
| `Host.Compilation` | `RECONSTRUIBLE` | 160 | 52 | 416 | `audits/TANDA-10-HOST-COMPILATION-AUDIT.md` |
| `Host.Driver` | `RECONSTRUIBLE` | 387 | 77 | 616 | `audits/TANDA-10-HOST-DRIVER-AUDIT.md` |
| `Host.LanguageService` | `RECONSTRUIBLE` | 126 | 42 | 336 | `audits/TANDA-10-HOST-LANGUAGE-SERVICE-AUDIT.md` |
| `Host.nsc` | `RECONSTRUIBLE` | 18 | 20 | 160 | `audits/TANDA-10-HOST-NSC-AUDIT.md` |

`RECONSTRUIBLE` describe suficiencia documental para la revisión congelada. No significa que la suite nueva
esté implementada ni que se haya ejecutado validación.

<!-- BEGIN TANDA-12A-SELFHOST-RECONSTRUCTIBILITY -->
## Componentes self-host auditados en Tanda 12A

| Componente | Resultado | Archivos | Declaraciones | Contratos | Casos diseñados | Evidencia |
|---|---|---:|---:|---:|---:|---|
| `SelfHost.Basic` | `RECONSTRUIBLE` | 3 | 38 | 4 | 28 | `audits/TANDA-12A-SELFHOST-BASIC-AUDIT.md` |
| `SelfHost.Bootstrap` | `RECONSTRUIBLE` | 1 | 4 | 3 | 23 | `audits/TANDA-12A-SELFHOST-BOOTSTRAP-AUDIT.md` |
| `SelfHost.CodeGen` | `RECONSTRUIBLE` | 1 | 122 | 19 | 136 | `audits/TANDA-12A-SELFHOST-CODEGEN-AUDIT.md` |
| `SelfHost.Diagnostics` | `RECONSTRUIBLE` | 1 | 15 | 4 | 28 | `audits/TANDA-12A-SELFHOST-DIAGNOSTICS-AUDIT.md` |
| `SelfHost.Documentation` | `RECONSTRUIBLE` | 1 | 0 | 2 | 12 | `audits/TANDA-12A-SELFHOST-DOCUMENTATION-AUDIT.md` |
| `SelfHost.Driver` | `RECONSTRUIBLE` | 1 | 26 | 10 | 79 | `audits/TANDA-12A-SELFHOST-DRIVER-AUDIT.md` |
| `SelfHost.IR` | `RECONSTRUIBLE` | 1 | 110 | 12 | 92 | `audits/TANDA-12A-SELFHOST-IR-AUDIT.md` |
| `SelfHost.Lexer` | `RECONSTRUIBLE` | 4 | 43 | 8 | 53 | `audits/TANDA-12A-SELFHOST-LEXER-AUDIT.md` |
| `SelfHost.Parser` | `RECONSTRUIBLE` | 1 | 97 | 16 | 127 | `audits/TANDA-12A-SELFHOST-PARSER-AUDIT.md` |
| `SelfHost.Project` | `RECONSTRUIBLE` | 1 | 0 | 3 | 22 | `audits/TANDA-12A-SELFHOST-PROJECT-AUDIT.md` |
| `SelfHost.Projects` | `RECONSTRUIBLE` | 2 | 79 | 12 | 95 | `audits/TANDA-12A-SELFHOST-PROJECTS-AUDIT.md` |
| `SelfHost.Sema` | `RECONSTRUIBLE` | 7 | 163 | 20 | 153 | `audits/TANDA-12A-SELFHOST-SEMA-AUDIT.md` |
| `SelfHost.Source` | `RECONSTRUIBLE` | 2 | 39 | 8 | 61 | `audits/TANDA-12A-SELFHOST-SOURCE-AUDIT.md` |
| `SelfHost.Symbols` | `RECONSTRUIBLE` | 1 | 56 | 5 | 35 | `audits/TANDA-12A-SELFHOST-SYMBOLS-AUDIT.md` |
| `SelfHost.Syntax` | `RECONSTRUIBLE` | 1 | 50 | 5 | 36 | `audits/TANDA-12A-SELFHOST-SYNTAX-AUDIT.md` |
| `SelfHost.Types` | `RECONSTRUIBLE` | 1 | 32 | 7 | 47 | `audits/TANDA-12A-SELFHOST-TYPES-AUDIT.md` |

El dictamen es exclusivamente documental para la revisión auditada; no demuestra ejecución, cobertura ni equivalencia con la batería histórica.
<!-- END TANDA-12A-SELFHOST-RECONSTRUCTIBILITY -->
