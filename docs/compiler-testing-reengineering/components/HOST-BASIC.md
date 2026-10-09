# Host.Basic — inventario semántico y contrato reconstructivo

## Estado, alcance y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE` y `HECHO COMPROBADO`.
- **Revisión:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Componente documental:** `Host.Basic`.
- **Universo cerrado:** doce archivos, 939 líneas y 32,016 bytes.
- **Fuentes:** `include/nativesharp/Basic/**` y `src/Basic/**`.
- **Validación ejecutada:** ninguna; la Fase 1 es exclusivamente estática.

Este documento describe los contratos observables y los algoritmos presentes. No legitima cambiar API,
semántica, pruebas históricas ni infraestructura.

## 1. Propósito y problema resuelto

`Host.Basic` reúne valores y servicios transversales que no pertenecen a una fase concreta del compilador:

1. identidad de archivos y coordenadas de fuente;
2. categorías de eventos de compilación;
3. códigos de salida del proceso;
4. versión e identidad irrepetible de una construcción;
5. SHA-256 canónico para artefactos y manifiestos;
6. publicación transaccional de uno o varios artefactos.

La frontera es deliberadamente pequeña. Los consumidores reciben valores estables o servicios con resultados
deterministas; `Basic` no conoce parser, semántica, LLVM, driver ni política de comandos.

## 2. Archivos y responsabilidades

| ID | Archivo | Responsabilidad |
|---|---|---|
| PF-000084 | `include/nativesharp/Basic/ArtifactTransaction.h` | API de publicación transaccional |
| PF-000085 | `include/nativesharp/Basic/BuildId.h.in` | plantilla CMake de identidad generada |
| PF-000086 | `include/nativesharp/Basic/CompilationEvent.h` | vocabulario de eventos de progreso |
| PF-000087 | `include/nativesharp/Basic/ExitCode.h` | contrato de códigos de proceso |
| PF-000088 | `include/nativesharp/Basic/FileId.h` | identidad compacta de archivo |
| PF-000089 | `include/nativesharp/Basic/Sha256.h` | API SHA-256 |
| PF-000090 | `include/nativesharp/Basic/SourceLocation.h` | coordenada byte dentro de un archivo |
| PF-000091 | `include/nativesharp/Basic/SourceRange.h` | intervalo medio abierto en un archivo |
| PF-000092 | `include/nativesharp/Basic/Version.h` | versiones, etapa e identidad visible |
| PF-000626 | `src/Basic/ArtifactTransaction.cpp` | staging, preflight, commit y rollback |
| PF-000627 | `src/Basic/Sha256.cpp` | algoritmo SHA-256 y hashing de archivos |
| PF-000628 | `src/Basic/Version.cpp` | materialización de valores generados |

## 3. Valores fundamentales

### 3.1 `FileId`

`FileId` encapsula un `uint32_t`. El valor cero es el único identificador inválido. El constructor por defecto
produce cero; el constructor explícito conserva sin transformación el valor recibido. `isValid()` compara con
cero, `value()` expone el entero y la igualdad compara el estado completo.

**Invariantes:**

- invalidez si y sólo si `value == 0`;
- no existe asignación global ni registro dentro del tipo;
- la relación entre valor y archivo pertenece a `SourceManager`;
- operaciones `constexpr` y `noexcept` no producen efectos laterales.

### 3.2 `SourceLocation`

Una ubicación combina `FileId` y offset byte `uint32_t`. Su validez depende sólo del identificador; el tipo no
comprueba que el offset pertenezca al buffer. Esa validación se difiere al consumidor que posee el archivo.

**Invariantes:**

- la ubicación por defecto es inválida y tiene offset cero;
- un `FileId` válido produce una ubicación válida incluso si el offset rebasa el archivo;
- igualdad significa mismo identificador y mismo offset;
- el offset se interpreta como byte, no carácter Unicode, línea ni columna.

### 3.3 `SourceRange`

El rango contiene `begin` y `end` y usa semántica de intervalo medio abierto `[begin, end)`. Es válido sólo si
ambos extremos son válidos, pertenecen al mismo `FileId` y `begin.offset <= end.offset`. Un rango vacío con
extremos iguales es válido.

El tipo no comprueba el tamaño del archivo; `SourceManager::getText` realiza esa comprobación.

### 3.4 `CompilationEvent`

`CompilationEventKind` contiene ocho estados observables:

| Valor | Nombre estable |
|---|---|
| `PhaseStarted` | `phase-started` |
| `PhaseFinished` | `phase-finished` |
| `WorkStarted` | `work-started` |
| `WorkFinished` | `work-finished` |
| `CacheHit` | `cache-hit` |
| `CacheMiss` | `cache-miss` |
| `Skipped` | `skipped` |
| `Failed` | `failed` |

`compilationEventKindName` devuelve `unknown` para un valor fuera del enum. `CompilationEvent` transporta el
tipo, fase, detalle, progreso `current/total` y duración en nanosegundos. Sus defaults representan el inicio de
una fase sin detalle, progreso ni duración.

### 3.5 `ExitCode`

El contrato numérico es público y observable por shell:

| Valor | Entero | Significado |
|---|---:|---|
| `Success` | 0 | operación completada |
| `CompilationError` | 1 | entrada o compilación controladamente inválida |
| `CommandLineError` | 2 | invocación o argumentos inválidos |
| `InternalError` | 70 | fallo interno o crash boundary |

## 4. Identidad de versión y construcción

### 4.1 Constantes estables

`Version.h` define `kLanguageVersion = 3.0.0`, `kCompilerReleaseVersion = 3.0.0`,
`kPreviousCompilerReleaseVersion = 2.0.0` y `kCompilerStage = 70` en la revisión congelada. Son datos del
producto actual, no reglas para rechazar proyectos por edición.

### 4.2 Identidad generada

`BuildId.h.in` es una plantilla. CMake sustituye `@NSC_BUILD_ID@` y `@NSC_COMPILER_VERSION@` para producir el
header consumido por `Version.cpp`. Éste publica:

- `kCompilerBuildId = detail::kGeneratedCompilerBuildId`;
- `kCompilerVersion = detail::kGeneratedCompilerVersion`.

El comentario contractual distingue la identidad estable de release de la identidad única de un binario. La
versión generada agrega metadata SemVer a la versión de release; el build ID evita que dos construcciones sean
indistinguibles.

**Dependencias:** productor `nsc_build_identity`; consumidor físico `nsc_basic`; consumidores funcionales en
Driver para banner, telemetría, bundles, compatibilidad y caché.

## 5. SHA-256

### 5.1 Entrada y salida

- `Sha256Digest` es exactamente 32 bytes.
- `sha256(span<uint8_t>)` devuelve el digest de los bytes recibidos.
- `sha256File` devuelve éxito, digest, tamaño y error textual.
- `sha256Hex` produce exactamente 64 caracteres hexadecimales minúsculos.

### 5.2 Algoritmo

La implementación sigue SHA-256 en memoria:

1. calcula longitud original en bits con `uint64_t`;
2. reserva el múltiplo de 64 capaz de contener `0x80` y ocho bytes de longitud;
3. copia la entrada, añade el bit inicial de padding y codifica la longitud big-endian;
4. por bloque, carga dieciséis palabras big-endian y expande hasta 64;
5. ejecuta 64 rondas con las constantes estándar, `choice`, `majority` y rotaciones;
6. acumula ocho palabras de estado;
7. serializa el estado big-endian en 32 bytes.

`rotateRight` es un helper privado de tiempo constante respecto del tamaño de entrada. `sha256` cuesta `O(n)`
tiempo y `O(n)` memoria porque materializa todo el mensaje padded. `sha256File` también cuesta `O(n)` memoria:
lee el archivo byte por byte en un vector y después llama a `sha256`.

### 5.3 Errores de archivo

`sha256File` limpia `error` al entrar. Si no abre el archivo devuelve `false` y el texto
`cannot open file for SHA-256: '<path>'`. Si la lectura marca `bad()`, devuelve `false` con
`cannot read file for SHA-256: '<path>'`. Sólo en éxito asigna tamaño y digest; el contrato no promete modificar
esas salidas en fallo.

## 6. Publicación transaccional de artefactos

### 6.1 Objetivo y modelo de estado

`ArtifactTransaction` publica un conjunto como una sola operación lógica. Cada entrada conserva destino final y
stage. La instancia posee:

- `stagingRoot_`: directorio temporal aislado;
- `entries_`: orden de publicación;
- `committed_`: evita un segundo commit exitoso.

No se permite copiar la transacción. El destructor elimina recursivamente el staging root e ignora fallos de
limpieza. El contrato de atomicidad es lógico: ante un fallo durante instalación intenta restaurar todos los
destinos anteriores y comunica si el rollback quedó completo.

### 6.2 Creación del staging

`ensureStaging` resuelve `temp_directory_path` y ensaya hasta cien nombres:

`nsc-artifacts-<pid>-<ordinal>-<attempt>`

En Windows incorpora PID; fuera de Windows usa cero. `transactionOrdinal` es atómico y relaxed: sólo aporta
unicidad de nombres, no sincronización de estado. Una colisión `file_exists` avanza; cualquier otro error corta
el ciclo. En fallo limpia `stagingRoot_` y devuelve texto determinista con el error del filesystem.

### 6.3 Registro de entradas

`add(destination, stage, error)`:

1. limpia `error`;
2. rechaza destino vacío;
3. crea staging si hace falta;
4. produce `<root>/<index>/<basename-destino>`;
5. crea su directorio padre;
6. registra la entrada y devuelve el stage.

El subdirectorio por índice conserva el basename real dentro de PE, import libraries y PDB, pero evita colisión
entre artefactos del mismo nombre.

`addStaged(destination, stage, error)` acepta un stage producido externamente. Rechaza cualquiera de las dos
rutas vacía y no crea directorios ni comprueba existencia hasta `commit`.

### 6.4 Preflight del conjunto

Antes de reemplazar el primer destino, `commit` recorre todas las entradas:

1. cada stage debe ser archivo regular;
2. el directorio del destino debe existir o poder crearse;
3. el destino debe faltar o ser archivo regular, nunca directorio ni reparse point en Windows;
4. calcula rutas sibling `.new` y backup `.old` con un sufijo ordinal común;
5. en Windows, rechaza destinos de 32,767 o más unidades UTF-16;
6. en Windows, consulta espacio libre y compara contra el tamaño del stage;
7. elimina residuos sibling/backup del mismo nombre;
8. copia y fuerza flush del sibling antes de comenzar la instalación.

Si cualquier preflight falla, elimina todos los siblings y backups preparados y no altera destinos finales.

### 6.5 Variantes de plataforma

En Windows se normaliza a ruta absoluta extendida `\\?\` o `\\?\UNC\`. Las operaciones críticas usan
`CopyFileW`, `CreateFileW`, `FlushFileBuffers`, `MoveFileExW` con reemplazo y write-through, y
`GetFileAttributesW`.

Fuera de Windows se usan `std::filesystem::copy_file`, `rename`, `symlink_status` y `remove`. No hay preflight
explícito de espacio ni flush durable en esa rama. Esta diferencia es implementación vigente y debe reflejarse
como matriz de plataforma; no puede asumirse equivalencia de durabilidad sin evidencia adicional.

### 6.6 Instalación y rollback

Después del preflight, cada entrada transita:

`prepared -> destination-backed-up? -> installed`

Si el destino existe, se mueve a backup. Después se mueve el sibling a destino y se marca `installed`. En éxito
se eliminan backups, se marca `committed_` y se devuelve `true`.

Ante fallo, el rollback recorre las entradas en orden inverso:

1. elimina el destino si el sibling ya fue instalado;
2. restaura el backup si existía;
3. elimina sibling residual;
4. preserva el diagnóstico original;
5. añade `; rollback-complete` o `; rollback-incomplete`.

El rollback es best effort: `removeNoThrow` ignora el resultado; sólo un fallo al restaurar un backup cambia el
sufijo a `rollback-incomplete`.

### 6.7 Clasificación de errores Windows

`classifyWindowsOutputError` agrupa códigos Win32:

- éxito;
- límite de ruta;
- componente ausente;
- permiso;
- archivo bloqueado;
- espacio insuficiente;
- I/O genérico.

`failWin32` forma `<operación> failed for '<ruta>': <clase> (Win32 error <código>)`. La ruta del mensaje se
convierte a UTF-8 mediante `u8string`.

### 6.8 Helpers de una sola operación

`writeFileTransactionally` crea una transacción, escribe bytes con truncamiento en el stage, cierra y verifica
el stream, y luego hace commit. `writeTextTransactionally` conserva exactamente los bytes de `string_view`.

`seedArtifactStage` implementa dos rutas:

- fuente ausente: éxito sin crear el stage, denominado cold path;
- fuente regular presente: crea el directorio y copia/flush al stage.

Rutas vacías son error. En Windows, un fallo de inspección es error; fuera de Windows, una fuente no regular sin
error se trata como ausencia.

## 7. Ownership, lifetime y thread safety

- `FileId`, `SourceLocation`, `SourceRange`, enums y eventos son valores sin ownership externo.
- `CompilationEvent` guarda `string_view`; el productor debe mantener vivas fase y detalle mientras se consuma.
- `ArtifactTransaction` posee `entries_` y `stagingRoot_`; no es copiable ni declara seguridad concurrente.
- `transactionOrdinal` permite nombres concurrentes sin serializar transacciones completas.
- `sha256` no conserva estado global y es reentrante.
- `sha256File` y las transacciones dependen del filesystem mutable; carreras externas pueden invalidar el
  preflight antes de la instalación.
- las vistas devueltas por constantes y nombres apuntan a almacenamiento estático.

## 8. Dependencias e integración

| Productor | Consumidores principales | Contrato |
|---|---|---|
| `FileId`/ubicaciones | Source, Lexer, AST, Diagnostics, Sema, HIR, MIR, Frontend | identidad y rangos byte |
| eventos | Driver y telemetría | nombres estables y progreso |
| `ExitCode` | Driver, main y crash boundary | clasificación observable del proceso |
| versión/build ID | Driver, caché, bundles y consola | compatibilidad e identidad de binario |
| SHA-256 | manifiestos, caché y determinismo | digest canónico |
| transacciones | publicación de objeto, PE, PDB, NSMETA, bundles y manifiestos | set completo o rollback |

## 9. Estados, errores y formatos

Los formatos estables de este componente son:

- nombres kebab-case de eventos;
- enteros de `ExitCode`;
- digest SHA-256 binario de 32 bytes y hexadecimal minúsculo de 64;
- strings de clasificación de fallo de salida;
- sufijos temporales `.nsc-transaction-<ordinal>.new/.old`;
- metadata SemVer de identidad generada.

Los errores se devuelven por `bool`/`optional` y un `std::string&`; no se lanzan deliberadamente. Operaciones de
`std::filesystem`, asignación y streams todavía podrían propagar excepciones del runtime C++ porque las APIs no
están marcadas `noexcept`.

## 10. Alternativas y decisiones observables

No existe evidencia en estos doce archivos de una evaluación histórica completa de alternativas. Sí existen
decisiones explícitas en comentarios y estructura:

- preservar basename en staging evita nombres internos obsoletos en PE/PDB;
- usar subdirectorios por entrada evita colisiones sin renombrar artefactos;
- copiar a sibling antes del primer reemplazo separa preflight de commit;
- recorrer rollback en reversa conserva dependencias del orden de instalación;
- build ID generado elimina el riesgo de una constante actualizada manualmente.

Cualquier motivación adicional es `NO VERIFICADA` hasta auditar historia, fuera de esta fase actual.

## 11. Pruebas históricas observadas

Como referencia congelada, no como suite nueva:

- `SourceManagerTests` y `SourceUnitTests` ejercitan valores de ubicación indirectamente;
- `NativeSharp3T34LongPathPublicationTests` cubre rutas largas, Unicode, clasificación y rollback completo;
- `Stage64InstallationManifestTests` cubre vectores SHA-256 y hashing de un archivo;
- Driver y telemetría consumen versión, build ID, eventos y códigos de salida.

La cobertura histórica no cierra los contratos. Faltan, entre otros, padding SHA-256 en cada frontera de bloque,
fallos de lectura después de abrir, `rollback-incomplete`, segundo commit, transacción vacía, ramas no Windows,
todos los códigos Win32 y lifetime de `string_view`.

## 12. Guía de reimplementación

Un reemplazo compatible debe implementarse en este orden:

1. tipos de valor y nombres canónicos;
2. identidad generada por el build;
3. SHA-256 contra vectores estándar;
4. abstracción de filesystem por plataforma;
5. staging aislado y registro de entradas;
6. preflight total sin tocar destinos;
7. instalación ordenada con backups;
8. rollback inverso y diagnóstico compuesto;
9. helpers de escritura y seed;
10. integración con consumidores sin cambiar formatos.

No se puede sustituir la transacción multiarchivo por renames independientes, ni el hash por una API con
representación diferente, ni `SourceRange` por índices de caracteres.

## 13. Respuestas de reconstructibilidad

| Pregunta normativa | Respuesta y sección |
|---:|---|
| 1. ¿Por qué existe? | Sección 1 |
| 2. ¿Qué problema resuelve? | Secciones 1 y 6.1 |
| 3. ¿Qué entradas recibe? | Secciones 3 a 6 |
| 4. ¿Qué salidas produce? | Secciones 3 a 6 y 9 |
| 5. ¿Qué contratos mantiene? | Secciones 3 a 9 |
| 6. ¿Qué estructuras usa? | Secciones 3, 5 y 6.1 |
| 7. ¿Qué algoritmos ejecuta? | Secciones 5.2 y 6.2 a 6.6 |
| 8. ¿Cuáles son sus invariantes? | Secciones 3, 6 y 7 |
| 9. ¿Cómo maneja errores? | Secciones 5.3, 6.7 y 9 |
| 10. ¿Qué estados/transiciones tiene? | Secciones 6.1 y 6.6 |
| 11. ¿Ownership/lifetime? | Sección 7 |
| 12. ¿Interacciones? | Sección 8 |
| 13. ¿Formatos? | Sección 9 |
| 14. ¿Complejidad? | Secciones 5.2 y 6; lineal en bytes o entradas |
| 15. ¿Alternativas? | Sección 10; evidencia histórica parcial |
| 16. ¿Por qué esta implementación? | Sección 10 |
| 17. ¿Cómo evolucionó? | No aplica al contrato de la revisión congelada; no se hace una afirmación histórica |
| 18. ¿Defectos históricos? | No aplica al diseño nuevo; la sección 11 sólo registra evidencia vigente |
| 19. ¿Qué pruebas lo demuestran? | Sección 11 y test plan separado |
| 20. ¿Cómo reimplementarlo? | Sección 12 |

## 14. Resultado de reconstructibilidad

`RECONSTRUIBLE` para la revisión congelada.

La arquitectura, contratos vigentes, algoritmos, errores, formatos e integración están descritos. La matriz
contiene 60 símbolos y 16 contratos; el test plan diseña 88 casos. La segunda auditoría independiente está en
`audits/TANDA-03-HOST-BASIC-AUDIT.md`. Este resultado no afirma historia completa ni pruebas ejecutadas.
