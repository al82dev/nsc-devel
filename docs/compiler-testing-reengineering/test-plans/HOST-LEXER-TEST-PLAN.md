# Test plan — Host.Lexer

## Estado

- **Clasificación:** `DISEÑO PROPUESTO`.
- **Implementación/ejecución:** ninguna.
- **Unidad lógica:** un caso puede ser fila parametrizada, no ejecutable separado.
- **Oráculo:** token kind, rango, lexema, alternativa/valor, diagnósticos y estado posterior.

## Contratos

| Contrato | Superficie |
|---|---|
| CL-LEX-001 | 150 kinds, nombres y clasificadores |
| CL-LEX-002 | Token/TokenValue/ownership |
| CL-LEX-003 | dump byte a byte |
| CL-LEX-004 | cursor, BOM, trivia, EOF y lexAll |
| CL-LEX-005 | identificadores, keywords y aliases |
| CL-LEX-006 | enteros, bases, sufijos y overflow |
| CL-LEX-007 | flotantes, fracción, exponente y sufijo |
| CL-LEX-008 | carácter y escapes byte |
| CL-LEX-009 | string ordinario y UTF-8 |
| CL-LEX-010 | raw strings |
| CL-LEX-011 | interpolated strings |
| CL-LEX-012 | puntuación y maximal munch |
| CL-LEX-013 | estado contextual assembly |
| CL-LEX-014 | diagnósticos, rangos y recuperación |

## Catálogo, Token y dump

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LEX-001 | CL-LEX-001 | los 152 valores | `tokenKindName` exacto y único | PR |
| TL-LEX-002 | CL-LEX-001 | valor desconocido | nombre `Invalid` | PR |
| TL-LEX-003 | CL-LEX-001 | rango KwVoid..KwString | todos y sólo ellos son builtin | PR |
| TL-LEX-004 | CL-LEX-001 | Owned Borrowed y vecinos | sólo dos qualifiers true | PR |
| TL-LEX-005 | CL-LEX-001 | `=` y diez compuestos y `??=` | assignment true | PR |
| TL-LEX-006 | CL-LEX-001 | tokens no assignment | false | PR |
| TL-LEX-007 | CL-LEX-001 | diez compuestos clásicos | base exacta | PR |
| TL-LEX-008 | CL-LEX-001 | `=` `??=` y otros | base `Invalid` | PR |
| TL-LEX-009 | CL-LEX-002 | token default-value variant | monostate y campos exactos | PR |
| TL-LEX-010 | CL-LEX-002 | valores uint64 double uint8 string | alternativa y valor exactos | PR |
| TL-LEX-011 | CL-LEX-002 | string temporal como value | token posee string tras morir entrada | PR |
| TL-LEX-012 | CL-LEX-002 | lexeme sobre source vivo | vista y rango exactos | PR |
| TL-LEX-013 | CL-LEX-003 | tokens simples | formato kind lexeme y offsets exacto | PR |
| TL-LEX-014 | CL-LEX-003 | LF CR tab slash quote | escapes exactos | PR |
| TL-LEX-015 | CL-LEX-003 | bytes 00 1f 7f 80 ff | `\xhh` minúsculo dos dígitos | PR |
| TL-LEX-016 | CL-LEX-003 | varios tokens/files | conserva orden; omite FileId y value | PR |

## Cursor, trivia, EOF e identificadores

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LEX-017 | CL-LEX-004 | fuente vacía | EOF rango 0:0 y lexAll tamaño 1 | PR |
| TL-LEX-018 | CL-LEX-004 | sólo BOM UTF-8 | EOF rango 3:3 | PR |
| TL-LEX-019 | CL-LEX-004 | BOM después de byte | tres bytes tratados normalmente/invalid según scanner | PR |
| TL-LEX-020 | CL-LEX-004 | espacios tab VT FF CR LF | todos omitidos y siguiente rango físico | PR |
| TL-LEX-021 | CL-LEX-004 | `//text` con LF | comentario omitido y LF consumido como trivia | PR |
| TL-LEX-022 | CL-LEX-004 | `//text` con CRLF | mismo contrato y offsets físicos | PR |
| TL-LEX-023 | CL-LEX-004 | `/*text*/` | omitido | PR |
| TL-LEX-024 | CL-LEX-004 | `/* outer /* inner */ x` | no anida; termina en primer cierre | PR |
| TL-LEX-025 | CL-LEX-004 | trivia alternada con tokens | sólo tokens reales más EOF | PR |
| TL-LEX-026 | CL-LEX-004 | llamadas nextToken después de EOF | EOF repetible sin avance | PR |
| TL-LEX-027 | CL-LEX-004 | lexAll de N tokens | orden y un EOF final | PR |
| TL-LEX-028 | CL-LEX-005 | `_ A z a0 _9` | Identifier con lexema/rango exacto | PR |
| TL-LEX-029 | CL-LEX-005 | dígito inicial | número seguido de identificador según bytes | PR |
| TL-LEX-030 | CL-LEX-005 | no-ASCII como inicio | Invalid byte(s), no Identifier Unicode | PR |
| TL-LEX-031 | CL-LEX-005 | 97 spellings | kind exacto de tabla | PR |
| TL-LEX-032 | CL-LEX-005 | diez aliases enteros | kind canónico compartido | PR |
| TL-LEX-033 | CL-LEX-005 | cambio de case | Identifier salvo spelling exacto existente | PR |
| TL-LEX-034 | CL-LEX-005 | `C` | Identifier contextual, no KwC | PR |
| TL-LEX-035 | CL-LEX-005 | prefijos/sufijos de keywords | Identifier completo | PR |

## Enteros y flotantes

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LEX-036 | CL-LEX-006 | `0 1 18446744073709551615` | uint64 exactos | PR |
| TL-LEX-037 | CL-LEX-006 | uint64 máximo + 1 | Invalid y overflow diagnostic | PR |
| TL-LEX-038 | CL-LEX-006 | `0x0 0Xff 0xABC` | valores base 16 | PR |
| TL-LEX-039 | CL-LEX-006 | `0x` | Invalid; requiere hex digit | PR |
| TL-LEX-040 | CL-LEX-006 | `0b0 0B1 0b1010` | valores base 2 | PR |
| TL-LEX-041 | CL-LEX-006 | `0b` | Invalid; requiere bit | PR |
| TL-LEX-042 | CL-LEX-006 | `0b102` | token binario válido hasta 0 y token posterior | PR |
| TL-LEX-043 | CL-LEX-006 | `0x1g` | entero 1 y Identifier g | PR |
| TL-LEX-044 | CL-LEX-006 | u U l L | sufijo aceptado y valor sin sufijo | PR |
| TL-LEX-045 | CL-LEX-006 | ocho combinaciones ul/lu case | aceptadas | PR |
| TL-LEX-046 | CL-LEX-006 | `uu ll uul` y corridas | consume corrida; Invalid suffix | PR |
| TL-LEX-047 | CL-LEX-006 | `1_000` | entero 1 e Identifier `_000` | PR |
| TL-LEX-048 | CL-LEX-007 | `1.0 0.5 12.25` | double exacto/tolerancia definida | PR |
| TL-LEX-049 | CL-LEX-007 | `1.` | entero 1 y Dot | PR |
| TL-LEX-050 | CL-LEX-007 | `1e2 1E-2 1e+2` | FloatingLiteral y valor | PR |
| TL-LEX-051 | CL-LEX-007 | `1e 1e+` | Invalid exponent diagnostic | PR |
| TL-LEX-052 | CL-LEX-007 | `1f 1F 1.5f 1e2f` | FloatingLiteral; suffix incluido en lexema | PR |
| TL-LEX-053 | CL-LEX-007 | `.5` | Dot e IntegerLiteral 5 | PR |
| TL-LEX-054 | CL-LEX-007 | `1e309` | caracteriza resultado de strtod vigente | PR |
| TL-LEX-055 | CL-LEX-007 | locale decimal no C controlado | riesgo de strtod caracterizado por plataforma | NIGHTLY |

## Caracteres y escapes

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LEX-056 | CL-LEX-008 | `'A' ' ' '~'` | CharacterLiteral byte exacto | PR |
| TL-LEX-057 | CL-LEX-008 | todos escapes simples | bytes 0 7 8 12 10 13 9 11 slash quotes | PR |
| TL-LEX-058 | CL-LEX-008 | `\x00 \x7f \xFF` en char | byte exacto | PR |
| TL-LEX-059 | CL-LEX-008 | `\x0` o no-hex | escape diagnostic y carácter Invalid | PR |
| TL-LEX-060 | CL-LEX-008 | escape desconocido | dos diagnósticos y token Invalid | PR |
| TL-LEX-061 | CL-LEX-008 | `''` | InvalidCharacterLiteral | PR |
| TL-LEX-062 | CL-LEX-008 | `'ab'` | recupera hasta cierre y token Invalid | PR |
| TL-LEX-063 | CL-LEX-008 | comilla sin cierre antes de EOL | no consume EOL; Invalid | PR |
| TL-LEX-064 | CL-LEX-008 | byte control/no-ASCII directo | Invalid | PR |

## Strings ordinarios y raw

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LEX-065 | CL-LEX-009 | `""` y ASCII | string value exacto | PR |
| TL-LEX-066 | CL-LEX-009 | escapes simples y hex | decoded bytes exactos | PR |
| TL-LEX-067 | CL-LEX-009 | UTF-8 válido de 2/3/4 bytes | bytes preservados | PR |
| TL-LEX-068 | CL-LEX-009 | overlong C0/C1 | InvalidStringLiteral | PR |
| TL-LEX-069 | CL-LEX-009 | surrogate ED A0.. | Invalid | PR |
| TL-LEX-070 | CL-LEX-009 | mayor que U+10FFFF | Invalid | PR |
| TL-LEX-071 | CL-LEX-009 | continuación ausente/incorrecta | Invalid sin perder progreso | PR |
| TL-LEX-072 | CL-LEX-009 | control directo | Invalid | PR |
| TL-LEX-073 | CL-LEX-009 | newline/EOF sin cierre | Invalid y newline queda para trivia | PR |
| TL-LEX-074 | CL-LEX-009 | escape desconocido | escape diagnostic más string Invalid | PR |
| TL-LEX-075 | CL-LEX-010 | triple quotes vacío | StringLiteral string vacío | PR |
| TL-LEX-076 | CL-LEX-010 | delimitador de 3 4 5 quotes | mismo ancho requerido para cierre | PR |
| TL-LEX-077 | CL-LEX-010 | slash y comillas por debajo del ancho | preservados literales | PR |
| TL-LEX-078 | CL-LEX-010 | tab CR LF | aceptados y preservados | PR |
| TL-LEX-079 | CL-LEX-010 | otro control | token Invalid al cerrar | PR |
| TL-LEX-080 | CL-LEX-010 | UTF-8 válido/inválido | misma matriz de límites y bytes | PR |
| TL-LEX-081 | CL-LEX-010 | cierre con quotes extra | consume ancho exacto; resto tokeniza después | PR |
| TL-LEX-082 | CL-LEX-010 | EOF sin cierre | Invalid y diagnóstico raw exacto | PR |

## Interpolated, operadores y errores

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LEX-083 | CL-LEX-011 | `$"text"` | un token interpolated y string value | PR |
| TL-LEX-084 | CL-LEX-011 | hole simple y anidado | llaves preservadas; profundidad equilibrada | PR |
| TL-LEX-085 | CL-LEX-011 | `{{` y `}}` fuera de hole | dobles preservados sin cambiar profundidad | PR |
| TL-LEX-086 | CL-LEX-011 | escape fuera de hole | decodificado | PR |
| TL-LEX-087 | CL-LEX-011 | slash y byte dentro de hole | ambos preservados | PR |
| TL-LEX-088 | CL-LEX-011 | `}` suelta con cierre quote | Invalid; caracteriza diagnóstico vigente | PR |
| TL-LEX-089 | CL-LEX-011 | hole sin cerrar hasta quote/newline | Invalid y diagnóstico cuando aplica | PR |
| TL-LEX-090 | CL-LEX-011 | EOF/newline sin cierre | InvalidStringLiteral | PR |
| TL-LEX-091 | CL-LEX-011 | bytes UTF-8/control en template | preservación vigente sin validación léxica | PR |
| TL-LEX-092 | CL-LEX-012 | cada token de un carácter | kind lexema rango exactos | PR |
| TL-LEX-093 | CL-LEX-012 | cada operador de dos caracteres | maximal munch exacto | PR |
| TL-LEX-094 | CL-LEX-012 | `... .. ..= .` | selección por longitud exacta | PR |
| TL-LEX-095 | CL-LEX-012 | `??= ?? ?. ?` | selección exacta | PR |
| TL-LEX-096 | CL-LEX-012 | `<<= >>= << >> <= >=` | selección exacta | PR |
| TL-LEX-097 | CL-LEX-012 | `-> => ++ --` | selección exacta | PR |
| TL-LEX-098 | CL-LEX-014 | byte inesperado ASCII | Invalid rango de un byte y UnexpectedCharacter | PR |
| TL-LEX-099 | CL-LEX-014 | varios inválidos separados | recuperación progresa y llega a EOF | PR |
| TL-LEX-100 | CL-LEX-014 | error limit del engine alcanzado | tokens Invalid siguen produciéndose aunque diagnóstico se suprima | PR |

## Assembly contextual

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LEX-101 | CL-LEX-013 | `asm {x}` | Identifier asm LeftBrace AssemblyText RightBrace EOF | PR |
| TL-LEX-102 | CL-LEX-013 | trivia/comentarios entre asm y brace | activa modo | PR |
| TL-LEX-103 | CL-LEX-013 | token real entre asm y brace | cancela modo | PR |
| TL-LEX-104 | CL-LEX-013 | `asm` al EOF | cancela espera y emite EOF | PR |
| TL-LEX-105 | CL-LEX-013 | líneas LF CR CRLF | AssemblyNewLine conserva bytes exactos | PR |
| TL-LEX-106 | CL-LEX-013 | texto con espacios/comments/quotes | AssemblyText conserva bytes sin trivia | PR |
| TL-LEX-107 | CL-LEX-013 | llaves anidadas | forman parte del text y protegen cierre externo | PR |
| TL-LEX-108 | CL-LEX-013 | llave en string/comment assembly | también cambia profundidad según bytes crudos | PR |
| TL-LEX-109 | CL-LEX-013 | fragmento antes del cierre externo | text primero y RightBrace en siguiente llamada | PR |
| TL-LEX-110 | CL-LEX-013 | EOF dentro del body | EOF sin diagnóstico assembly específico | PR |

## Criterio de cierre futuro

Los 110 casos deben implementarse o justificarse por fila. Las tablas completas de kinds/keywords/operators se
generarán desde oráculos independientes; las pruebas no duplicarán el mismo switch. Cada caso comprobará
progreso a EOF para impedir loops silenciosos. Las matrices de locale y streams se reportarán por plataforma.
