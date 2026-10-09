# Host.Source — inventario semántico y contrato reconstructivo

## Estado, alcance y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE` y `HECHO COMPROBADO`.
- **Revisión:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Componente documental:** `Host.Source`.
- **Universo cerrado:** cinco archivos, 330 líneas y 8,794 bytes.
- **Fuentes:** `include/nativesharp/Source/**` y `src/Source/**`.
- **Validación ejecutada:** ninguna; la Fase 1 es exclusivamente estática.

Este documento describe la implementación congelada. No cambia el compilador, sus pruebas históricas ni la
infraestructura que las ejecuta.

## 1. Propósito y frontera

`Host.Source` convierte bytes de entrada en una representación estable para las fases posteriores. Sus
responsabilidades son:

1. poseer ruta, nombre visible y bytes de cada unidad fuente;
2. asignar un `FileId` compacto y recuperable;
3. indexar líneas con reglas explícitas para `LF`, `CR` y `CRLF`;
4. traducir offsets byte a coordenadas 1-based;
5. entregar vistas de rangos, líneas y rutas sin copiar;
6. leer archivos completos con un límite preventivo de 64 MiB.

No decodifica Unicode, no tokeniza, no interpreta sintaxis y no valida que los bytes formen texto. Todas las
coordenadas se miden sobre bytes.

## 2. Archivos y responsabilidades

| ID | Archivo | Responsabilidad |
|---|---|---|
| PF-000347 | `include/nativesharp/Source/SourceFile.h` | API y almacenamiento de una unidad fuente |
| PF-000348 | `include/nativesharp/Source/SourceManager.h` | lectura, registro, búsqueda y extracción |
| PF-000349 | `include/nativesharp/Source/SourcePosition.h` | coordenada línea/columna 1-based |
| PF-000854 | `src/Source/SourceFile.cpp` | indexación de líneas y conversión de offsets |
| PF-000855 | `src/Source/SourceManager.cpp` | lectura exacta y resolución por `FileId` |

## 3. Modelo de datos y ownership

### 3.1 `SourcePosition`

Es un valor agregado con `line = 1` y `column = 1` por defecto. No contiene `FileId`, no valida rangos y no
distingue por sí mismo una coordenada real de la respuesta fallback para una ubicación inválida.

### 3.2 `SourceFile`

El constructor mueve ruta y buffer hacia almacenamiento propio, materializa `displayPath_` mediante
`path_.string()` y construye una tabla `lineStarts_`. Siempre existe al menos una entrada de línea, el offset
cero, incluso para un archivo vacío.

La instancia posee:

- `id_`: identidad asignada externamente;
- `path_`: ruta de filesystem;
- `displayPath_`: representación estrecha congelada al construir;
- `buffer_`: bytes exactos de entrada;
- `lineStarts_`: offsets de comienzo de cada línea lógica.

`displayPath()` y `buffer()` devuelven `string_view`; `path()` devuelve referencia. No transfieren ownership.

### 3.3 `SourceManager`

Posee un `vector<SourceFile>`. El identificador asignado es `files_.size() + 1`, por lo que el índice físico es
`id.value() - 1`. Cero permanece reservado como sentinel inválido.

Los punteros retornados por `getFile` son no propietarios. Una inserción posterior puede realocar el vector e
invalidar punteros y referencias a sus elementos. Las vistas retornadas apuntan a almacenamiento poseído por
un `SourceFile`; el contrato seguro no permite conservarlas más allá de una mutación estructural o destrucción
del manager. La implementación no declara seguridad concurrente.

## 4. Indexación de líneas

### 4.1 Construcción iterativa

`buildLineStarts` limpia la tabla, inserta cero y recorre el buffer sin recursión:

- ante `\r`, avanza una posición y consume además un `\n` inmediato;
- ante `\n` aislado, avanza una posición;
- después de cada terminador registra el siguiente offset como comienzo de línea;
- cualquier otro byte sólo avanza el cursor.

Por tanto, `CRLF` cuenta como un solo salto; `CR` y `LF` aislados también. Un terminador al final crea una línea
final vacía. El algoritmo cuesta `O(n)` tiempo y `O(L)` almacenamiento para `L` líneas.

### 4.2 Tamaño y número de líneas

`size()` devuelve el tamaño en bytes convertido a `uint32_t`; si el string fuese mayor que `UINT32_MAX`, lo
satura a ese máximo. La ruta normal de `readFileBuffer` nunca llega a esa condición porque limita a 64 MiB,
pero `addBuffer` acepta un string proporcionado por el llamador.

`lineCount()` devuelve el tamaño de `lineStarts_` convertido a `uint32_t`. Con el límite de offsets y la
representación vigente, el componente presupone que ese conteo cabe en 32 bits.

### 4.3 Offset a posición

`positionForOffset` primero limita el offset a `size()`. Luego usa `upper_bound` sobre `lineStarts_` para hallar
el último comienzo no mayor que el offset. Devuelve línea e índice de columna sumando uno.

Consecuencias observables:

- offset cero es `1:1`;
- un offset mayor al buffer se trata como EOF;
- EOF después de un terminador pertenece a la línea final vacía;
- columna cuenta bytes desde el comienzo de línea, no code points;
- complejidad `O(log L)`.

### 4.4 Texto de una línea

`lineText` rechaza línea cero y cualquier línea mayor que `lineCount`. Para una línea válida toma el intervalo
entre dos comienzos consecutivos, o hasta EOF en la última línea. Después retira del extremo todos los bytes
`CR` y `LF` contiguos y devuelve una vista del contenido restante.

La línea vacía y una línea inválida producen ambas una vista vacía; el llamador debe usar el número o el conteo
si necesita distinguirlas.

## 5. Lectura exacta de archivos

`readFileBuffer` abre en modo binario, consulta `file_size` y rechaza:

- apertura fallida: `cannot open source file '<ruta>'`;
- error al consultar tamaño: `cannot determine source file size for '<ruta>'`;
- más de 64 MiB: `source file exceeds the 64 MiB safety limit`;
- lectura corta, stream fallido o bytes adicionales tras el tamaño consultado:
  `cannot read source file '<ruta>'`;
- tamaño superior al límite de offset de 32 bits:
  `source file is larger than the stage 10 32-bit source-offset limit`.

El último control es redundante para archivos leídos bajo el límite de 64 MiB, pero forma parte de la
implementación vigente. El método reserva exactamente `sourceBytes`, lee esa cantidad y exige EOF inmediato;
detecta así cambios de tamaño durante la lectura. Cuesta `O(n)` tiempo y `O(n)` memoria.

La función no limpia `errorMessage` al entrar ni en éxito. El contrato vigente sólo asigna el error en fallo;
un texto previo puede sobrevivir a una lectura exitosa.

`loadFile` compone lectura y `addBuffer`. Si leer falla, no agrega ninguna unidad. Si funciona, mueve los bytes
y devuelve el nuevo identificador.

## 6. Registro, búsqueda y extracción

### 6.1 Registro e identidad

`addBuffer` asigna identificadores consecutivos desde uno y mueve ruta y buffer al vector. La operación es
amortizadamente `O(1)` más el `O(n)` de construir el índice de líneas. No existe deduplicación por ruta ni
comprobación explícita de overflow de `files_.size() + 1` antes de convertir a `uint32_t`.

### 6.2 Búsqueda

`getFile` devuelve `nullptr` para cero o un valor mayor que el número de archivos; en otro caso devuelve el
elemento correspondiente en `O(1)`.

`getPosition` devuelve `SourcePosition{1,1}` si el archivo no existe. Para un archivo válido delega la
conversión y, por tanto, limita offsets fuera de rango a EOF.

### 6.3 Rangos y vistas

`getText` exige un `SourceRange` válido, un archivo existente y que `end.offset <= file.size()`. El contrato de
`SourceRange` ya asegura mismo archivo y orden. En éxito devuelve exactamente `[begin,end)`; un rango vacío
válido produce una vista vacía indistinguible por contenido de un rechazo.

`getLineText` requiere archivo válido, calcula la posición —con clamp a EOF— y devuelve esa línea sin
terminadores. `getPath` devuelve el `displayPath` congelado o una vista vacía para archivo inválido.

Las consultas son `noexcept`; no reportan una categoría de error. Sus sentinels son `nullptr`, posición 1:1 o
vista vacía según la API.

## 7. Invariantes y transiciones

| Invariante/transición | Regla |
|---|---|
| identificador | cero inválido; archivos registrados usan 1..N |
| registro | `N -> N+1`; el nuevo ID corresponde al último elemento |
| tabla de líneas | no vacía, ordenada y comienza en cero |
| terminadores | CRLF uno; CR uno; LF uno |
| coordenadas | línea y columna 1-based; columnas en bytes |
| offset externo | se limita a EOF sólo al pedir posición/línea |
| rango | extracción rechaza cualquier fin posterior a EOF |
| archivo leído | snapshot exacto del tamaño observado o fallo completo |
| ownership | manager/archivo poseen; punteros, referencias y vistas no |

No hay rollback: una lectura falla antes de insertar y una inserción exitosa permanece hasta destruir el
manager.

## 8. Dependencias e integración

| Productor/consumidor | Relación |
|---|---|
| `Host.Basic` | provee `FileId`, `SourceLocation` y `SourceRange` |
| Lexer/Parser | consumen bytes, posiciones y líneas para tokenización y errores |
| Diagnostics | consume ruta, línea y texto para renderizar evidencia |
| Frontend/Project | carga buffers de unidades y conserva IDs |
| AST/Sema/HIR/MIR | transportan ubicaciones/rangos producidos sobre esta tabla |
| filesystem/ifstream | producen snapshot binario y errores de entrada |

Una interpretación de offsets distinta en cualquier consumidor rompe diagnósticos, slices y metadatos aunque
el archivo compile.

## 9. Casos límite y riesgos observados

- archivo vacío: una línea, tamaño cero, posición EOF `1:1`;
- terminador final: agrega una línea vacía adicional;
- múltiples terminadores consecutivos: agregan líneas vacías individuales según CRLF/CR/LF;
- NUL y bytes no UTF-8 se conservan;
- `displayPath_` depende de la conversión estrecha de `filesystem::path` en la plataforma;
- `addBuffer` no aplica el límite de 64 MiB ni protege overflow de IDs;
- una carrera que cambia el archivo durante lectura produce fallo si tamaño/contenido ya no coincide;
- `errorMessage` no se limpia en éxito;
- conservar `SourceFile*`, referencias o vistas mientras se agregan archivos no tiene garantía de estabilidad.

Son hechos o riesgos derivados de la implementación; este documento no autoriza corregirlos durante Fase 1.

## 10. Evidencia histórica observada

Sin modificarla, la suite congelada contiene `SourceUnitTests` y consumidores indirectos en Diagnostics,
Lexer, Parser y Frontend. Esa evidencia no demuestra por sí sola cada contrato nuevo: faltan matrices completas
de terminadores, carreras de lectura, lifetime, límites, bytes arbitrarios y sentinels ambiguos.

## 11. Guía de reimplementación

Un reemplazo compatible debe implementar en este orden:

1. `SourcePosition` y la relación 1-based;
2. ownership de ruta y bytes en `SourceFile`;
3. escaneo iterativo CR/LF/CRLF;
4. búsqueda binaria de offset y extracción sin terminadores;
5. asignación consecutiva y lookup por `FileId`;
6. validación de rangos y vistas no propietarias;
7. lectura binaria exacta con límite y textos de error;
8. integración con Diagnostics y Lexer usando offsets byte.

No puede sustituirse el buffer por texto normalizado, contar caracteres Unicode en columnas, eliminar la línea
vacía final ni ocultar el clamp a EOF sin cambiar comportamiento observable.

## 12. Respuestas de reconstructibilidad

| Pregunta normativa | Respuesta y sección |
|---:|---|
| 1-2. propósito/problema | Sección 1 |
| 3-4. entradas/salidas | Secciones 3 a 6 |
| 5. contratos | Secciones 4 a 7 |
| 6-7. estructuras/algoritmos | Secciones 3 y 4 |
| 8. invariantes | Sección 7 |
| 9. errores | Secciones 5, 6 y 9 |
| 10. estados | Sección 7 |
| 11. ownership/lifetime | Sección 3 |
| 12. interacciones | Sección 8 |
| 13. formatos | Secciones 4 y 5 |
| 14. complejidad | Secciones 4 a 6 |
| 15-18. alternativas/evolución/defectos | No verificados históricamente; riesgos vigentes en sección 9 |
| 19. evidencia | Sección 10 y test plan separado |
| 20. reimplementación | Sección 11 |

## 13. Resultado de reconstructibilidad

`RECONSTRUIBLE` para la revisión congelada, condicionado a conservar semántica byte, ownership y sentinels.

El cierre cuantitativo y la segunda auditoría independiente se registran en
`audits/TANDA-03-HOST-SOURCE-AUDIT.md`. No se afirma que las pruebas estén implementadas ni ejecutadas.
