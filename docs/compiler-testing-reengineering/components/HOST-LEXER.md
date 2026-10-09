# Host.Lexer — inventario semántico y contrato reconstructivo

## Estado y alcance

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE` y `HECHO COMPROBADO`.
- **Revisión:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Universo:** ocho archivos, 1,699 líneas y 51,554 bytes.
- **Catálogo:** 150 `TokenKind`, 97 spellings reservados y 87 clases keyword distintas.
- **Validación ejecutada:** ninguna; sólo auditoría estática y diseño.

## 1. Propósito y frontera

`Host.Lexer` recorre iterativamente los bytes de un `SourceFile` y produce tokens con rangos byte, lexema no
propietario y valor decodificado opcional. También emite diagnósticos léxicos. No construye AST, no decide
precedencia y no valida la gramática de expresiones interpoladas o assembly.

## 2. Archivos

| ID | Archivo | Responsabilidad |
|---|---|---|
| PF-000260 | `include/nativesharp/Lexer/Lexer.h` | API, helpers y estado del scanner |
| PF-000261 | `include/nativesharp/Lexer/Token.h` | token y variante de valor |
| PF-000262 | `include/nativesharp/Lexer/TokenDumper.h` | dump textual estable |
| PF-000263 | `include/nativesharp/Lexer/TokenKind.h` | catálogo y clasificadores |
| PF-000774 | `src/Lexer/Lexer.cpp` | reconocimiento completo |
| PF-000775 | `src/Lexer/Token.cpp` | ownership/accesores de token |
| PF-000776 | `src/Lexer/TokenDumper.cpp` | escape y formato de dump |
| PF-000777 | `src/Lexer/TokenKind.cpp` | nombres, rangos y operadores |

## 3. Catálogo de tokens

Los 150 valores abarcan EOF/Invalid, identificadores, seis literales, dos records de assembly, keywords,
delimitadores y operadores. Los tokens añadidos al final (`InterpolatedStringLiteral`, `??`, `??=`, `?.`,
`..`, `..=`) preservan identidades numéricas anteriores.

`tokenKindName` devuelve el nombre del enumerador para los 150 valores y `Invalid` para uno desconocido.

Clasificadores:

- builtin: rango ordinal inclusivo `KwVoid..KwString`;
- ownership: sólo `KwOwned` y `KwBorrowed`;
- asignación: `=`, el rango `+=..>>=` y `??=`;
- base compuesta: diez mappings aritméticos/bitwise; `=` y `??=` devuelven `Invalid`.

Aunque existe `KwC`, la tabla del lexer no reserva `C`: se produce `Identifier`. Es un token contextual vigente,
documentado también por la infraestructura congelada; no es una omisión inferida a corregir aquí.

## 4. Token, valor y lifetime

`TokenValue` es `monostate | uint64_t | double | uint8_t | string`. `Token` copia kind/range/lexeme y mueve la
variante. El lexema es `string_view` al buffer del `SourceFile`; el valor string sí es propietario.

| Kind | Alternativa esperada |
|---|---|
| entero | `uint64_t` |
| flotante | `double` |
| carácter | `uint8_t` |
| string/raw/interpolated | `string` decodificado |
| restantes | `monostate` |

El `SourceFile` debe sobrevivir a tokens y lexer. El lexer guarda referencias no propietarias a archivo y
engine, más una vista del buffer; no es thread-safe.

## 5. Cursor, rangos y secuencia

El constructor detecta exclusivamente BOM UTF-8 `EF BB BF` en offset cero y comienza en tres; los rangos
siguen usando offsets físicos. `lexAll` llama `nextToken` hasta incluir exactamente un EOF.

`current`/`peek` devuelven NUL fuera del buffer; `advance` no rebasa EOF; `match` sólo avanza al coincidir.
`range(begin,end)` usa el `FileId` de entrada y `text` toma `[begin,end)`. El scanner avanza de forma iterativa.

Antes de cada token normal se eliminan espacio, tab, tab vertical, form feed, CR, LF y comentarios. `//` deja el
terminador para la siguiente iteración de trivia. `/* */` no anida. Si llega a EOF emite un error
`UnterminatedBlockComment` y la próxima salida es EOF, no un token Invalid.

Complejidad total normal `O(n)` amortizada, más lookup promedio `O(1)` de keyword; memoria `O(T + valores)`.

## 6. Identificadores y keywords

Inicio permitido: ASCII `A-Z`, `a-z`, `_`; continuación añade `0-9`. No se aceptan letras Unicode. El lexema se
consulta en una tabla estática de 97 spellings. Diez aliases enteros (`i8/u8/.../isize/usize`) comparten clases
con tipos canónicos, de ahí 87 `TokenKind` keyword distintos.

La comparación es exacta y case-sensitive. Cualquier spelling no presente, incluido `C`, queda `Identifier`.

## 7. Números

### 7.1 Enteros

- decimal por defecto;
- `0x`/`0X` requiere al menos un hex digit;
- `0b`/`0B` requiere al menos un bit;
- sufijo opcional `u/U`, `l/L` o ambas letras en cualquier orden/case;
- una tercera letra o duplicado inválido consume toda la corrida `uUlL` y reporta sufijo inválido;
- `from_chars` debe consumir todos los dígitos y caber en `uint64_t`.

No existen separators. Un carácter que no pertenece al radix termina el token y se procesa después; por
ejemplo `0b102` se separa según el prefijo válido en vez de validar toda la secuencia como una sola unidad.

### 7.2 Flotantes

Sólo parten de dígito decimal. La fracción requiere `.` seguido de dígito; el exponente requiere dígitos después
de signo opcional; `f/F` fuerza clase flotante. Se construye una copia NUL-terminated y `strtod` produce el
`double`. El control exige que haya consumo, no que `strtod` alcance el final; esto permite el sufijo `f` ya
consumido por el scanner.

## 8. Carácter y escapes

Un carácter contiene exactamente un ASCII imprimible permitido o un escape. Valores directos válidos están en
`0x20..0x7e`, excepto slash y comilla simple que deben escaparse. El valor es un byte.

Escapes: `0 a b f n r t v \\ \' \"` y `\xHH` con exactamente dos hex digits. `\x` incompleto reporta
`InvalidEscapeSequence` en la ubicación inicial del literal; un escape desconocido lo reporta en el byte del
escape. El literal inválido se recupera hasta comilla o fin de línea y emite además `InvalidCharacterLiteral`.

## 9. Strings

### 9.1 Ordinario

Termina en comilla antes de CR/LF/EOF. Decodifica escapes a bytes. ASCII de control directo lo vuelve inválido.
UTF-8 se conserva byte a byte y se valida contra secuencias de 2/3/4 bytes, excluyendo overlongs, surrogates y
valores posteriores a U+10FFFF mediante límites del segundo byte. Un fallo produce token Invalid y
`InvalidStringLiteral`; un escape inválido puede haber emitido previamente `InvalidEscapeSequence`.

### 9.2 Raw

Se activa con tres o más comillas; toda la corrida inicial fija el ancho de delimitador. Conserva contenido sin
escapes, acepta tab/CR/LF y aplica la misma validación UTF-8. Otros controles invalidan. Una corrida de comillas
de ancho suficiente cierra consumiendo exactamente el delimitador; comillas extra quedan para el siguiente
token. EOF reporta raw string unterminated.

### 9.3 Interpolado

Comienza `$"` y produce un único `InterpolatedStringLiteral` con la plantilla completa decodificada. Mantiene
profundidad de llaves; `{{` y `}}` fuera de hole se conservan dobles. Fuera de hole decodifica escapes; dentro
preserva slash y el byte siguiente para que una capa posterior procese la expresión. Una `}` sin apertura marca
el token inválido. El lexer no valida sintaxis interna, UTF-8 ni controles de la plantilla interpolada.

Si una plantilla inválida alcanza comilla de cierre, retorna `Invalid`; los escapes inválidos ya tienen su
diagnóstico específico, mientras una llave cierre suelta no añade en ese camino un diagnóstico genérico. Si
llega a CR/LF/EOF, reporta `InvalidStringLiteral`.

## 10. Operadores y puntuación

Se aplica maximal munch para delimitadores de uno a tres bytes: `...` precede a `..`/`..=`, `??=` precede a
`??`, shifts compuestos preceden a shifts, y así sucesivamente. El catálogo cubre todos los valores de la sección
de puntuación del enum. Un byte no reconocido consume uno, reporta `UnexpectedCharacter` y produce `Invalid`.

## 11. Assembly contextual

El spelling `asm` sigue siendo `Identifier`. Después de emitirlo, el lexer espera el siguiente token real;
trivia y comentarios pueden intervenir. `{` activa modo assembly con profundidad uno; cualquier otro token o
EOF cancela la espera.

Dentro del cuerpo:

- cada terminador físico produce `AssemblyNewLine` conservando LF, CR o CRLF;
- cada fragmento restante de línea produce `AssemblyText` con bytes exactos;
- `{` y `}` internos ajustan profundidad y permanecen dentro del texto;
- el único `}` en profundidad uno sale como `RightBrace` normal y abandona modo assembly;
- strings/comentarios no tienen semántica especial: sus llaves también alteran profundidad;
- EOF interno produce EOF sin diagnóstico léxico específico.

## 12. Dump estable

Por token se escribe:

`<TokenKind> "<lexema escapado>" @<begin>:<end>\n`

El dump no incluye `FileId` ni `TokenValue`. Escapa LF, CR, tab, slash y comilla; ASCII imprimible queda literal y
otros bytes usan `\xhh` minúsculo de dos dígitos. Restaura base decimal y fill de espacio.

## 13. Diagnósticos y recuperación

| Condición | ID | Recuperación |
|---|---|---|
| comentario de bloque sin cierre | `UnterminatedBlockComment` | consume a EOF |
| número/prefijo/exponente/sufijo/overflow | `InvalidNumericLiteral` | token Invalid del fragmento consumido |
| carácter inválido | `InvalidCharacterLiteral` | busca comilla o EOL |
| escape inválido | `InvalidEscapeSequence` | consume escape disponible |
| string ordinario/raw/interpolado inválido | `InvalidStringLiteral` cuando aplica | token Invalid |
| byte inesperado | `UnexpectedCharacter` | consume un byte |

Todos son `Error` y usan offsets físicos del `SourceFile`.

## 14. Dependencias, riesgos y límites

- depende de Basic, Source y Diagnostics;
- `unordered_map` estático inicializa la tabla al primer uso;
- locale de `strtod` puede afectar interpretación decimal si el proceso altera locale;
- los rangos y lexemas dependen del lifetime del source;
- keywords y caracteres son ASCII, strings admiten UTF-8 validado;
- raw e interpolated tienen políticas distintas de controles/UTF-8;
- assembly cuenta llaves crudas, no estructura de strings/comentarios;
- no hay recursión en los ocho archivos.

Estos son contratos o riesgos vigentes; la Fase 1 no los cambia.

## 15. Evidencia histórica y reconstructibilidad

Las pruebas históricas de Lexer y dumps permanecen congeladas. Sirven como referencia, pero la nueva suite debe
separar catálogo, cada familia léxica, recuperaciones, rangos, valores y estados contextuales.

Una reimplementación compatible debe construir primero catálogo/Token, luego cursor/trivia, identificadores,
números, escapes, strings, operadores, assembly y dumper. No debe normalizar lexemas o EOL.

`RECONSTRUIBLE` quedará confirmado por `audits/TANDA-04-HOST-LEXER-AUDIT.md` después de cerrar matrices y segunda
revisión. No se implementaron ni ejecutaron pruebas.
