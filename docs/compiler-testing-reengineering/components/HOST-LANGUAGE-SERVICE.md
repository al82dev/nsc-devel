# Host.LanguageService — protocolo editorial, snapshots e índice semántico

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo secciones rotuladas como diseño futuro.
- **Revisión congelada:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Universo:** 4 archivos, 1,614 líneas y 63,887 bytes.
- **Inventario físico:** 126 símbolos: 28 tipos y 98 funciones, métodos, constructores o destructores.
- **Resultado:** 42 contratos estables y 336 casos lógicos diseñados.
- **Evidencia:** `evidence/TANDA-10-HOST-LANGUAGE-SERVICE-METHOD.md` y
  `audits/TANDA-10-HOST-LANGUAGE-SERVICE-AUDIT.md`.

## 2. Frontera del componente

`Host.LanguageService` es una fachada estable para consumidores editoriales. No expone directamente lexer,
parser, Sema ni los modelos de proyecto. Recibe documentos abiertos como overlays, solicita un análisis completo
de cuerpos de método y proyecta el resultado a tipos propios: diagnósticos, símbolos, definiciones, completions,
firmas, referencias, renombrado, tokens semánticos, formato y acciones de código.

La frontera comprende:

- `ProtocolTypes`: coordenadas UTF-16 y DTOs independientes del frontend;
- `LanguageService`: lifecycle de documentos y orquestación de análisis;
- `AnalysisSnapshot`: vista inmutable compartida de una generación;
- `SemanticIndex`: proyección privada de modelos frontend a operaciones editoriales.

No comprende transporte LSP/JSON-RPC, persistencia entre procesos, edición incremental del AST ni codegen.

## 3. Modelo de datos del protocolo

Las posiciones son cero-basadas y expresan columnas en unidades UTF-16. `TextRange` es medio abierto:
`start <= position < end`. Los DTOs conservan valores y defaults, no ejecutan validación propia.

`LanguageSymbol` separa:

- `location`: rango completo de la declaración;
- `selectionRange`: identificador exacto cuando pudo localizarse;
- `renameable`: verdadero únicamente cuando existe ese rango exacto.

Los kinds distinguen tipos, callables, datos y parámetros. Los edits siempre incluyen path y rango. El resultado
de rename distingue autorización, error y conjunto de ediciones; nunca usa un conjunto parcial como éxito.

## 4. Conversión UTF-8 y UTF-16

`decodeUtf8` reconoce secuencias de uno a cuatro bytes. Rechaza continuaciones inválidas, truncamiento,
overlongs, sustitutos UTF-16 y escalares mayores a `0x10FFFF`; el fallback es `U+FFFD` consumiendo un byte.

`utf16PositionForOffset` recorre iterativamente hasta el offset solicitado:

1. CRLF cuenta como una sola nueva línea;
2. CR y LF aislados también reinician columna;
3. un escalar BMP suma una unidad UTF-16;
4. un escalar suplementario suma dos;
5. un offset dentro de una secuencia devuelve `nullopt`.

`byteOffsetForUtf16Position` ejecuta la conversión inversa sólo en fronteras representables. Una columna que
cae entre las dos unidades de un escalar suplementario, una línea ausente o una columna más allá del final no
produce offset.

## 5. Diagnósticos

`makeLanguageDiagnostic` necesita que `SourceManager` conserve el archivo del diagnóstico y que ambos extremos
se conviertan a UTF-16. Preserva path, rango, mensaje y nombre estable de `DiagnosticId`; mapea `Note`, `Warning`
y el resto a `Error`. Un archivo o rango no materializable omite el diagnóstico en vez de inventar coordenadas.

## 6. Lifecycle de documentos

Los paths pasan por `filesystem::absolute`, normalización léxica, separadores genéricos y lowercase. En Windows,
spellings equivalentes comparten clave. Un path vacío o cuya absolutización falle es inválido.

La máquina de estado es:

```text
CLOSED --open(version >= 0)--> OPEN(version, text)
OPEN(v) --update(version > v)--> OPEN(newVersion, newText)
OPEN --close--> CLOSED
```

Abrir dos veces, actualizar un documento cerrado o usar una versión negativa/no creciente falla sin mutación.
`document()` devuelve una copia; `documentCount()` y toda mutación se protegen con el mismo mutex.

## 7. Formato y acciones de código

El formatter es deliberadamente conservador. Escanea líneas iterativamente, elimina whitespace final, conserva
LF frente a CRLF e indenta según balance de llaves. El scanner ignora llaves dentro de strings, caracteres,
comentarios de línea y comentarios de bloque; el estado de comentario de bloque cruza líneas. `tabSize` se
limita a `1..16`; tabs y espacios son configurables.

Si aparece el token `asm` seguido de `{` fuera de comentarios y literales, el documento se devuelve byte por
byte sin formatear. Así se evita reinterpretar el cuerpo de ensamblador con reglas del lenguaje anfitrión.

`formatDocument` emite una única edición de documento completo sólo cuando el texto cambia y el final se puede
representar en UTF-16. `codeActions` puede producir:

- quick fix `Remove unexpected character` para un rango no vacío y convertible;
- source fix-all `Format NativeSharp document` cuando el formatter produce una edición.

## 8. Solicitud de análisis

Cada llamada a `analyzeProject` reserva una generación atómica creciente. Antes de compilar:

1. canoniza el path del proyecto;
2. copia bajo mutex el mapa completo de documentos abiertos;
3. crea un `ProjectCompilation` nuevo;
4. limita jobs a un mínimo de uno;
5. instala el callback de cancelación;
6. instala un source provider que consulta la copia de overlays;
7. ejecuta `compileProjectMethodBodies`.

La copia desacopla la generación en curso de updates posteriores. Un overlay desconocido cae al proveedor normal
del compilador. El snapshot conserva la compilación actual aunque ésta falle, para mantener diagnósticos y
fuentes vivos.

## 9. Publicación y fallback semántico

Tras el análisis, los diagnósticos actuales siempre se convierten desde la compilación actual. Bajo mutex:

- éxito sin errores: se construye el índice y reemplaza `lastGoodSemantic[project]`;
- fallo o errores: se busca el último índice válido del mismo proyecto;
- si existe, se publica como modelo consultable y se marca `usedPreviousSemanticModel`;
- si no existe, las consultas semánticas permanecen neutrales.

Por tanto, `currentAnalysisSucceeded`, diagnósticos actuales y disponibilidad del modelo son dimensiones
independientes. El fallback no convierte un análisis fallido en éxito.

## 10. Construcción del índice semántico

El índice retiene `ProjectCompilation` mediante `shared_ptr`, garantizando lifetime de syntax y modelos. Exige
name lookup, member model, namespace-function model y method-body model; si alguno falta devuelve un índice
vacío válido.

Se materializan, en orden de los modelos:

- tipos nombrados no precompilados;
- miembros no precompilados ni generados por el compilador;
- parámetros de miembros;
- funciones y states de namespace no precompilados;
- locales de cuerpos.

Cada símbolo obtiene detalle estable, location amplia y, cuando el lexer encuentra un identificador coincidente
dentro del rango, selection exacta. La búsqueda exacta recorre tokens de la unidad con el mismo `FileId`.

## 11. Referencias y llamadas

Las referencias derivan de type bindings, name bindings, member calls, namespace function bindings y state
bindings. Una referencia conserva todos los targets candidatos; si no hay targets no se indexa. Primero intenta
un rango exacto de identificador y sólo entonces cae al rango sintáctico amplio, marcado como no exacto.

Las llamadas resueltas conservan:

- rango completo de la llamada;
- firma renderizada;
- rangos de argumentos;
- mapeo argumento a parámetro seleccionado.

Esto permite signature help para argumentos nombrados o reordenados sin volver a resolver semántica.

## 12. Consultas editoriales

- `documentSymbols`: filtra por path normalizado.
- `workspaceSymbols`: substring case-insensitive sobre qualified name, orden fuente y límite estricto.
- `symbolAt`: prioriza referencia; de no existir usa la declaración con menor rango.
- `definitions`: devuelve todos los targets candidatos o la ubicación del símbolo seleccionado.
- `completions`: limita locales/parámetros al documento, filtra prefix case-insensitive, deduplica
  `name + detail`, limita y ordena por label/detail.
- `signatureHelp`: elige la llamada contenedora más pequeña, calcula argumento activo, aplica el remap y limita
  el índice al último parámetro.
- `references`: expande overloads de método/función con mismo kind y qualified name, opcionalmente incorpora
  declaraciones y deduplica locations.

Todas devuelven vacío o `nullopt` cuando no existe modelo semántico; no lanzan ni consultan almacenamiento
destruido.

## 13. Renombrado

El nombre nuevo debe comenzar con letra ASCII o `_`, continuar con alfanuméricos o `_` y no coincidir con la
lista de keywords mantenida por la fachada. `prepareRename` exige targets renameables y, sobre una referencia,
un identificador exacto.

`rename` vuelve a validar, reúne declaraciones y referencias y deduplica por path/range. Si una sola referencia
carece de rango exacto, borra todas las ediciones y falla. La operación es lógicamente atómica: nunca autoriza
un subconjunto silencioso.

## 14. Tokens semánticos

Se proyectan declaraciones renameables y referencias exactas del documento. El kind de una referencia procede
de su primer target. La salida deduplica `range + kind + declaration` y se ordena por inicio y fin, lo que la hace
determinista para el adaptador editorial.

## 15. Ownership, concurrencia y complejidad

- `LanguageService` posee su implementación con `unique_ptr` y permite move, no copy.
- snapshots e índices comparten ownership con `shared_ptr`.
- documentos, cache y publicación se serializan con un mutex.
- generaciones usan un contador atómico relajado porque sólo requieren unicidad/orden de fetch.
- el compilador trabaja fuera del mutex; sólo la copia inicial y la publicación final bloquean.

Las conversiones UTF son `O(bytes)`. Formato es `O(bytes)`. Construcción del índice es lineal en símbolos y
bindings salvo búsquedas de identificador, que recorren tokens por símbolo y pueden acercarse a
`O(symbols * tokens)`. Consultas actuales recorren vectores linealmente; deduplicación de locations también
puede ser cuadrática en el número de resultados.

## 16. Errores e invariantes

Invariantes centrales:

- una versión abierta nunca decrece;
- una generación pertenece a una sola solicitud;
- diagnostics describen la compilación actual, aun con modelo anterior;
- un rename exitoso contiene sólo rangos exactos;
- índices retienen su compilación propietaria;
- paths se comparan mediante una clave canónica común;
- un formatter que detecta inline assembly no modifica el documento.

Errores se expresan como `false + string`, `nullopt`, colección vacía o flags del snapshot. No existe excepción
propia del componente ni publicación parcial deliberada.

## 17. Estrategia futura de pruebas

Los 42 contratos de `inventory/contracts.csv` poseen ocho casos cada uno: nominal mínimo, nominal
representativo, prerrequisito ausente, entrada inválida, borde inferior, borde superior, repetición determinista
y hostil/NIGHTLY. El plan completo está en `test-plans/HOST-LANGUAGE-SERVICE-TEST-PLAN.md`.

La futura suite debe separar helpers puros de un fixture de componente con proyecto real. Debe comprobar de
forma explícita el cruce UTF-8/UTF-16, overlays, fallback de último modelo válido, overload groups, referencias
inexactas, atomicidad de rename y aislamiento concurrente de snapshots.

## 18. Reconstructibilidad y cierre

La documentación identifica propósito, entradas, salidas, estructuras, algoritmos, estados, invariantes,
errores, ownership, concurrencia, complejidad, dependencias, formatos, observabilidad y estrategia de prueba.
Los cuatro archivos y 126 símbolos quedan trazados a 42 contratos y 336 casos.

`Host.LanguageService` queda `AUDITADO` y `RECONSTRUIBLE` para la revisión congelada. Esto no afirma ejecución,
cobertura instrumental ni ausencia de defectos del producto.
