# Tanda 02.01 — implementación completa de Host.Lexer

## SCOPE LOCK local

| Dimensión | Decisión |
|---|---|
| Ola | `2 — Sintaxis y semántica frontal` |
| Componente | `Host.Lexer` |
| Contratos | `CL-LEX-001..CL-LEX-014` |
| Casos | `TL-LEX-001..TL-LEX-110` |
| Producto | no modificar |
| Suite histórica | conservar intacta; no ejecutar |
| Configuraciones | Debug y Release; optimización `N/A` |
| Prohibido | baseline, cobertura, `C:`, despliegue y remotos |

## Implementación

Los 110 casos diseñados en la Fase 1 quedaron materializados en el ejecutable
consolidado `nsc_component_tests`. Cada caso conserva su identidad, contrato,
componente, capa y perfil en el manifiesto legible por máquina.

| Módulo | Casos | Cantidad | Contrato |
|---|---|---:|---|
| `LexerTokenKindModule.cpp` | `TL-LEX-001..008` | 8 | `CL-LEX-001` |
| `LexerTokenValueModule.cpp` | `TL-LEX-009..012` | 4 | `CL-LEX-002` |
| `LexerTokenDumperModule.cpp` | `TL-LEX-013..016` | 4 | `CL-LEX-003` |
| `LexerCursorTriviaModule.cpp` | `TL-LEX-017..027` | 11 | `CL-LEX-004` |
| `LexerIdentifierModule.cpp` | `TL-LEX-028..035` | 8 | `CL-LEX-005` |
| `LexerIntegerModule.cpp` | `TL-LEX-036..047` | 12 | `CL-LEX-006` |
| `LexerFloatingModule.cpp` | `TL-LEX-048..055` | 8 | `CL-LEX-007` |
| `LexerCharacterLiteralModule.cpp` | `TL-LEX-056..064` | 9 | `CL-LEX-008` |
| `LexerStringLiteralModule.cpp` | `TL-LEX-065..074` | 10 | `CL-LEX-009` |
| `LexerRawStringModule.cpp` | `TL-LEX-075..082` | 8 | `CL-LEX-010` |
| `LexerInterpolatedStringModule.cpp` | `TL-LEX-083..091` | 9 | `CL-LEX-011` |
| `LexerOperatorModule.cpp` | `TL-LEX-092..097` | 6 | `CL-LEX-012` |
| `LexerDiagnosticRecoveryModule.cpp` | `TL-LEX-098..100` | 3 | `CL-LEX-014` |
| `LexerAssemblyModule.cpp` | `TL-LEX-101..110` | 10 | `CL-LEX-013` |
| **Total** | `TL-LEX-001..110` | **110** | **14 contratos** |

`LexerTestSupport.h` contiene únicamente fixtures y aserciones compartidas para
fuente, tokens, rangos, valores y diagnósticos. No replica el scanner ni altera
estado privado del producto.

## Hallazgo conservado

`TL-LEX-075` materializa un defecto real: la secuencia de seis comillas que debe
representar una cadena raw vacía se consume actualmente como un delimitador
único de ancho seis y termina inválida. El oráculo correcto permanece rojo y el
caso está marcado `KNOWN_FAILURE` tanto en el registro C++ como en el manifiesto.
No se cambió el compilador ni se invirtió la expectativa.

## Auditoría de integración

La segunda pasada detectó y corrigió antes del commit dos errores documentales:

1. los tres módulos de catálogo estaban clasificados como capa `unit`, aunque el
   inventario canónico asigna todo `Host.Lexer` a `component`;
2. recuperación diagnóstica y ensamblado contextual tenían intercambiados los
   contratos `CL-LEX-013` y `CL-LEX-014`.

Las correcciones afectan exclusivamente metadata de la suite. Los oráculos y el
producto permanecen idénticos.

## Estado al crear el commit

- implementados: `110/110`;
- evidencia dirigida de los agentes: `109 PASS`, `1 KNOWN_FAILURE`;
- verificación integrada del target y del controlador: pendiente para ejecutarse
  después de este commit, conforme a la regla de no probar cambios sin guardarlos;
- limitaciones nuevas de infraestructura en Lexer: `0`.
