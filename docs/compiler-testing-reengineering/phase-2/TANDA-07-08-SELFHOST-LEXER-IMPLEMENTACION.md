# Tanda 07.08 — implementación e integración de SelfHost.Lexer

## SCOPE LOCK operativo

- **Objetivo:** materializar los 8 contratos y 53 casos canónicos de `SelfHost.Lexer` con el producto
  self-host real.
- **Naturaleza:** infraestructura de pruebas de Fase 2; producto sin cambios.
- **Revisión canónica:** `65835716712f0d04537aa92591f766fb65ff641b`.
- **Archivos autorizados:** fixture y módulos `.ns`, proyecto fixture, controlador, fragmento temporal,
  `CMakeLists.txt`, `manifest/cases.jsonl` y este documento.
- **Protegidos:** registros C++ y `main.cpp`, producto, históricos, coverage, bibliotecas externas,
  `TRACKING.md`, `CURSOR.json`, C: y remotos.
- **Prohibido:** build, pruebas, gates, validación dinámica y fallback a artefactos viejos.

## Denominador canónico

| Magnitud | Total |
|---|---:|
| Archivos de producto | 4 |
| Contratos | 8 |
| Casos | 53 |
| IDs | `TSH-00399..TSH-00451` |
| `unit` / `component` | 45 / 8 |
| `PR` / `NIGHTLY` | 52 / 1 |
| Ejecutor | `nsc_suite_controller+nsc` |

## Clasificación

| Disposición | Casos | IDs |
|---|---:|---|
| `Required` | 52 | todos salvo `TSH-00442` |
| `InfrastructurePending` | 0 | ninguno |
| `KnownProductDefect` | 1 | `TSH-00442` |

`TSH-00442` exige el raw string vacío de seis comillas. `LexRawString` cuenta las seis comillas contiguas
como un único delimitador de apertura de ancho seis, alcanza EOF y emite `Invalid`; no reconoce apertura y
cierre triples. La prueba conserva el oráculo `StringLiteral`, permanece ejecutable y el gate nunca transforma
su fallo en PASS o XFAIL exitoso.

## Contratos y módulos

| Contrato | IDs | Módulo | Oráculo |
|---|---|---|---|
| `CSH-LEX-001` | 399..405 | `SelfHostLexerVocabularyStreamModule.ns` | tabla ordinal/nombre exhaustiva |
| `CSH-LEX-002` | 406..412 | mismo | kinds, rangos, orden y contador de errores |
| `CSH-LEX-003` | 413..419 | `SelfHostLexerCursorTriviaModule.ns` | traza pública de cursor y clasificación |
| `CSH-LEX-004` | 420..425 | mismo | cursor final, siguiente token y errores |
| `CSH-LEX-005` | 426..431 | `SelfHostLexerIdentifiersNumbersModule.ns` | kind, lexema implícito por rango y keywords |
| `CSH-LEX-006` | 432..437 | mismo | kind, rango, cursor siguiente y error |
| `CSH-LEX-007` | 438..443 | `SelfHostLexerLiteralsModule.ns` | secuencia, rangos y errores de literales |
| `CSH-LEX-008` | 444..451 | `SelfHostLexerFullModule.ns` | secuencia, EOF, dump y errores |

`SelfHostLexerTestSupport.ns` crea `SourceBuffer`, ejecuta `Lexer.LexAll` y usa `TokenDumper.Dump`; no
reimplementa el lexer. `SelfHostLexer.nsproj` incluye las cuatro fuentes de producto y la clausura mínima
Basic/Source/Diagnostics, con Core, Collections.Generic y Core.IO.

## Reescritura de filas genéricas

Los textos automáticos `missing producer prerequisite` o `malformed relationship` no describen productores
opcionales de estas APIs. Se reescribieron contra conducta pública vigente: stream vacío y `AddError`, BOM,
carácter inválido, comentarios sin cierre, tokens contextuales, prefijos numéricos incompletos, escapes
inválidos, EOF y mutación parcial conservada. No se inventaron features pendientes.

## Manifiesto y gate

- `manifest/cases.jsonl`: 53 filas ejecutables y únicas, una por ID; sólo `TSH-00442` lleva
  `expected_outcome=KNOWN_FAILURE`. El fragmento temporal se retiró después de confirmar la fusión 53/53.
- `run_selfhost_lexer_gate.py`: valida el denominador, compila una vez la fixture y ejecuta un proceso por
  caso; conserva `PASS`, `FAIL`, `KNOWN_PRODUCT_DEFECT` y `UNEXPECTED_PASS`. Los dos últimos mantienen el gate
  rojo hasta reclasificación.
- `CMakeLists.txt`: registra exactamente un `nsc.compiler-suite.selfhost-lexer-controller`, condicionado a
  ejecutables explícitos de NSC y NSLink. Las fuentes `.ns` permanecen propiedad del `.nsproj`; no se
  introdujeron registros o fuentes proxy en el ejecutable C++.

## Estado de validación

No se ejecutaron build, pruebas, gates ni validación. Estado exacto:

`IMPLEMENTACIÓN E INTEGRACIÓN ESTÁTICAS COMPLETAS; VALIDACIÓN DE BUILD PENDIENTE DEL USUARIO`.
