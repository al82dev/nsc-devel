# Auditoría estática de completitud — Fase 2

## Alcance y resultado

Esta auditoría coteja la autoridad canónica de Fase 1 con la implementación de
Fase 2. El corte cubre exclusivamente NSC: `44` componentes, `1,208` contratos y
`9,587` casos de las olas 1–9.

| Métrica | Autoridad | Materializado | Integrado | Huecos |
|---|---:|---:|---:|---:|
| Componentes | 44 | 44 | 44 | 0 |
| Contratos | 1,208 | 1,208 | 1,208 | 0 |
| Casos | 9,587 | 9,587 | 9,587 | 0 |
| Filas ejecutables del manifiesto | 5,695 | 5,695 | 5,695 | 0 |

El manifiesto contiene `5,695` identificadores únicos y cero duplicados. Los
`3,892` casos restantes conservan una disposición no ordinaria en registros o
catálogos explícitos; no se presentan como `PASS`. La integración central
contiene `307` funciones de registro únicas y `115` controladores declarados.

Este es un cierre estático de implementación. No se ejecutaron builds, pruebas
ni gates. El contador conservador de casos validados permanece en `333`.

## Cotejo por componente

`Manifiesto` incluye casos `Required` y `KnownProductDefect` ejecutables. La
columna `No ejecutable` conserva casos que requieren infraestructura, contradicen
el plan o permanecen delegados a la batería histórica.

| Ola | Componente | Contratos | Previstos | Materializados | Integrados | Manifiesto | No ejecutable |
|---:|---|---:|---:|---:|---:|---:|---|
| 1 | Host.Basic | 16 | 88 | 88 | 88 | 88 | 0 |
| 1 | Host.Diagnostics | 12 | 75 | 75 | 75 | 75 | 0 |
| 1 | Host.Language | 21 | 286 | 286 | 286 | 286 | 0 |
| 1 | Host.Source | 8 | 58 | 58 | 58 | 54 | 4 InfrastructurePending |
| 1 | Host.Target | 30 | 240 | 240 | 240 | 240 | 0 |
| 2 | Host.AST | 20 | 230 | 230 | 230 | 224 | 6 InfrastructurePending |
| 2 | Host.Lexer | 14 | 110 | 110 | 110 | 110 | 0 |
| 2 | Host.Parser | 42 | 536 | 536 | 536 | 533 | 3 InfrastructurePending |
| 2 | Host.Sema | 30 | 321 | 321 | 321 | 313 | 8 InfrastructurePending |
| 3 | Host.CFG | 26 | 208 | 208 | 208 | 204 | 4 InfrastructurePending |
| 3 | Host.Dominance | 32 | 256 | 256 | 256 | 249 | 7 InfrastructurePending |
| 3 | Host.HIR | 29 | 232 | 232 | 232 | 232 | 0 |
| 3 | Host.MIR | 38 | 304 | 304 | 304 | 304 | 0 |
| 3 | Host.Passes | 56 | 448 | 448 | 448 | 442 | 6 InfrastructurePending |
| 3 | Host.SSA | 42 | 336 | 336 | 336 | 336 | 0 |
| 4 | Host.Compilation | 52 | 416 | 416 | 416 | 416 | 0 |
| 4 | Host.Frontend | 63 | 504 | 504 | 504 | 504 | 0 |
| 5 | Host.Interop | 48 | 384 | 384 | 384 | 160 | 224 InfrastructurePending |
| 5 | Host.Linker | 40 | 320 | 320 | 320 | 13 | 307 InfrastructurePending |
| 5 | Host.LLVM | 64 | 512 | 512 | 512 | 72 | 440 InfrastructurePending |
| 5 | Host.Runtime | 48 | 384 | 384 | 384 | 80 | 304 InfrastructurePending |
| 6 | Host.Driver | 77 | 616 | 616 | 616 | 264 | 352 InfrastructurePending |
| 6 | Host.LanguageService | 42 | 336 | 336 | 336 | 256 | 80 InfrastructurePending |
| 6 | Host.nsc | 20 | 160 | 160 | 160 | 0 | 160 InfrastructurePending |
| 7 | SelfHost.Basic | 4 | 28 | 28 | 28 | 27 | 1 PlanContradiction |
| 7 | SelfHost.Bootstrap | 3 | 23 | 23 | 23 | 0 | 23 InfrastructurePending |
| 7 | SelfHost.CodeGen | 19 | 136 | 136 | 136 | 0 | 136 InfrastructurePending |
| 7 | SelfHost.Diagnostics | 4 | 28 | 28 | 28 | 28 | 0 |
| 7 | SelfHost.Documentation | 2 | 12 | 12 | 12 | 0 | 12 InfrastructurePending |
| 7 | SelfHost.Driver | 10 | 79 | 79 | 79 | 0 | 79 InfrastructurePending |
| 7 | SelfHost.IR | 12 | 92 | 92 | 92 | 0 | 92 InfrastructurePending |
| 7 | SelfHost.Lexer | 8 | 53 | 53 | 53 | 53 | 0 |
| 7 | SelfHost.Parser | 16 | 127 | 127 | 127 | 0 | 127 InfrastructurePending |
| 7 | SelfHost.Project | 3 | 22 | 22 | 22 | 0 | 22 InfrastructurePending |
| 7 | SelfHost.Projects | 12 | 95 | 95 | 95 | 0 | 95 InfrastructurePending |
| 7 | SelfHost.Sema | 20 | 153 | 153 | 153 | 0 | 153 InfrastructurePending |
| 7 | SelfHost.Source | 8 | 61 | 61 | 61 | 48 | 2 InfrastructurePending + 11 PlanContradiction |
| 7 | SelfHost.Symbols | 5 | 35 | 35 | 35 | 0 | 35 InfrastructurePending |
| 7 | SelfHost.Syntax | 5 | 36 | 36 | 36 | 0 | 36 InfrastructurePending |
| 7 | SelfHost.Types | 7 | 47 | 47 | 47 | 36 | 1 InfrastructurePending + 10 PlanContradiction |
| 8 | SelfHost.Bootstrap.Infrastructure | 73 | 438 | 438 | 438 | 0 | 438 InfrastructurePending |
| 8 | SelfHost.Contracts | 48 | 288 | 288 | 288 | 0 | 288 InfrastructurePending |
| 9 | BuildSystem | 76 | 456 | 456 | 456 | 36 | 420 HistoricalDelegated |
| 9 | Packaging | 3 | 18 | 18 | 18 | 12 | 6 InfrastructurePending |

## Cierre de los cuatro casos Host.Source

Los cuatro casos son contratos válidos del producto actual. No son defectos del
compilador ni expectativas inventadas; requieren una infraestructura que no
existe en el perfil ordinario. Por eso su disposición correcta es
`InfrastructurePending` y permanecen fuera del manifiesto ejecutable.

| Caso | Contrato | Clasificación | Oráculo conservado | Infraestructura necesaria |
|---|---|---|---|---|
| `TS-SRC-008` | `CS-SRC-002` | InfrastructurePending | `SourceFile::size()` satura en `UINT32_MAX` | fixture acotado que represente más de 4 GiB sin poseer ni recorrer ese buffer |
| `TS-SRC-040` | `CS-SRC-006` | InfrastructurePending | truncar después de `file_size` produce fallo sin valor parcial | seam determinista entre consulta de tamaño y lectura |
| `TS-SRC-041` | `CS-SRC-006` | InfrastructurePending | crecer después de `file_size` es detectado por el `peek` final | seam determinista entre consulta, lectura y comprobación final |
| `TS-SRC-049` | `CS-SRC-007` | InfrastructurePending | una referencia invalidada por realocación no puede reutilizarse | build ASan versionado y ejecución aislada en subproceso |

Los casos se registran en los módulos existentes de `Host.Source`; por tanto ya
quedan enlazados por los registros y fuentes CMake centrales existentes. El gate
de ownership coteja el denominador canónico de `58`, exige `54` filas de
manifiesto y confirma que el host publique los `58` identificadores.

## Hueco adicional detectado y cerrado

`TF-LANGUAGE-SERVICE-304` ya existía en el módulo C++ como
`KnownProductDefect`, pero faltaba en el manifiesto y el gate sólo enumeraba
`TF-LANGUAGE-SERVICE-297..303`. El caso ahora está integrado como
`KNOWN_FAILURE`; conserva el oráculo de rechazo atómico del nombre reservado
`i32`, sin ediciones parciales. `Host.LanguageService` queda en `336/336`:
`255 Required`, `1 KnownProductDefect` ejecutable y `80 InfrastructurePending`.

## Evidencia estática

- Autoridad: `implementation-waves.csv`, `test-case-design.csv` y
  `suite-case-allocation.csv`.
- Manifiesto: `5,695` filas no vacías, `5,695` identificadores únicos, cero
  duplicados.
- `Host.Source`: `58` casos registrados, `54` filas ejecutables y `4`
  `InfrastructurePending`.
- `Host.LanguageService`: `256` filas ejecutables; exactamente una conserva
  `expected_outcome=KNOWN_FAILURE`.
- El diff no contiene errores de espacios. No se ejecutaron pruebas.
