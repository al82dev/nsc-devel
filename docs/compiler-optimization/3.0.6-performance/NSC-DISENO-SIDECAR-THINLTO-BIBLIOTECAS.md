# NSC — Diseño del sidecar ThinLTO de bibliotecas estáticas

**Fecha:** 2026-10-25
**Rama:** `development-fixes-debug-ide`
**Plan:** optimización parte 2, punto 47 (diseño), punto 48 (consumo), puntos 50 a 53 (internalización, paquetes e
identidad completa)
**Antecedente:** `NSC-AUDITORIA-LTO-2026-10-24.md`, sección 8, LTO-03

## 1. Objetivo

Una biblioteca estática construida con `-flto=thin` publica, junto a su `.lib`, el bitcode de pre-enlace de cada
unidad de código. Un ejecutable o una DLL construidos con `-flto=thin` pueden entonces importar e insertar en línea
funciones de esa biblioteca durante sus backends ThinLTO, en lugar de llamar a los objetos nativos ya cerrados.

El sidecar es una aceleración opcional. El contrato de biblioteca de tres artefactos (`.lib`, `.nsmeta` y runtime
opcional) no cambia, NSMETA no se versiona y el objeto nativo sigue siendo siempre un respaldo válido.

## 2. Alcance

| Caso | Decisión |
|---|---|
| Productor: biblioteca estática con ThinLTO efectivo | Publica `<Biblioteca>.nsthinlto` (ver sección 3). |
| Productor: DLL | No publica sidecar: el límite de la DLL es un contrato binario y no debe importar cuerpos. |
| Consumidor: ejecutable o DLL con ThinLTO efectivo | Consume cada sidecar compatible de sus referencias estáticas. |
| Consumidor: biblioteca estática | No consume: su `.lib` lleva sólo su código; el enlace final ve cada referencia. |
| Referencias a DLL (`runtimePath` no vacío) | No se consideran. |
| Paquetes restaurados (`nsc restore`) | Desde el punto 52 el sidecar se restaura junto a la `.lib` estática. |
| `-O0`, `-flto=none`, `-flto=full`, host DYNAMIC | No hay ThinLTO efectivo: ni se produce ni se consume. |

ThinLTO efectivo significa `-flto=thin`, nivel `-O1` a `-O3` y host STATIC (`ThinLtoBackend::available()`).

## 3. Ubicación

El productor sólo publica sidecar cuando su `.lib` contiene exactamente los objetos de sus unidades, es decir,
cuando el runtime de NativeSharp está configurado como biblioteca aparte; si el runtime se empaqueta dentro de la
`.lib`, no hay sidecar. El digest de biblioteca liga esa garantía al archivo publicado.

El sidecar vive junto a la biblioteca: `Biblioteca.lib` → `Biblioteca.nsthinlto` (`replace_extension`). Se publica
en la misma transacción de artefactos que la `.lib`, el `.h`, la `.nsmeta` y el `.nsproducts`. Cuando una
construcción posterior de la misma biblioteca no tiene ThinLTO efectivo, el sidecar publicado antes se elimina
después del commit; aunque quedara, su enlace criptográfico con la `.lib` lo rechazaría.

Cuando la biblioteca lo produce, el sidecar es una salida requerida del bundle de compilación: restaurar el bundle
exige que exista y que su digest coincida.

## 4. Formato v2

Todos los enteros son little-endian. Una cadena es `u32 longitud` seguido de esa cantidad de bytes UTF-8.

| Campo | Contenido |
|---|---|
| Firma | 8 bytes `NSTHINLT`. |
| Versión | `u32` = 2. |
| Compatibilidad | Cadena: identidad de compatibilidad (sección 5). |
| Digest de biblioteca | Cadena: SHA-256 hexadecimal de los bytes de la `.lib` publicada. |
| Digest de metadata | Cadena: SHA-256 hexadecimal de los bytes de la `.nsmeta` publicada. |
| Dependencias | Cadena: una línea `nombre digest` por biblioteca referenciada por el productor, ordenadas. |
| Unidades | `u32` número de unidades, luego por unidad `u64` tamaño y el bitcode. |
| Integridad | 32 bytes: SHA-256 de todos los bytes anteriores. |

Cada unidad es exactamente el bitcode que el productor entregó a sus propios backends: el módulo después del
pipeline `thinlto-pre-link<On>`, con su resumen de módulo y su hash de módulo (`GenerateHash`). El orden de las
unidades es el ordinal de partición del productor.

El formato v1 (punto 48) no tenía dependencias; el lector lo rechaza por su versión y la biblioteca se enlaza nativa.
El lector rechaza: firma o versión distintas, longitudes que exceden el archivo, más de 65 536 unidades, unidades
vacías, bytes sobrantes y una integridad que no coincide.

## 5. Identidad de compatibilidad

Texto de líneas `clave valor`, comparado byte a byte:

```
llvm <LLVM_VERSION_STRING>
llvm-revision <LLVM_REVISION, o none si el LLVM no la registra>
triple <triple del target>
cpu <CPU del target>
features <cadena de features>
layout <data layout>
optimization O<n>
debug <none|line-tables-only|full>
codegen <kCodegenCacheIdentity>
policy <codeGenerationPolicyIdentity(thin)>
pipeline <nombre del pipeline> <versión del pipeline>
floating-point <modo> <contracción FMA> <redondeo> <desnormalizados>
runtime <SHA-256 de la biblioteca de runtime configurada>
```

La versión y la revisión exactas de LLVM, el target (triple, CPU, features y data layout), el nivel y el pipeline
de optimización, el tipo de información de depuración, la identidad de codegen de este compilador, la política de
generación de código, el contrato de punto flotante y la biblioteca de runtime de NativeSharp deben coincidir.
Cualquier diferencia rechaza el sidecar completo; no hay compatibilidad parcial.

`kCodegenCacheIdentity` no es una versión: es el SHA-256 del contenido de los fuentes de AST, Language, Frontend,
Target, LLVM, Runtime y Linker del compilador. Cambia con cualquier cambio de código que pueda alterar el codegen y
no con el sello de cada build del ejecutable.

Auditoría de opciones (punto 53): las demás opciones de `nsc` no cambian el bitcode de una biblioteca ya construida
ni la forma en que el consumidor lo compila. `-fdebug-prefix-map` sólo reescribe rutas de la depuración propia de
cada producto; `--link-lib`, `/subsystem:`, `--incremental-link` y `--library-reference-root` son del enlace o de
la resolución de rutas; los límites de genéricos, `--time`, `--progress`, `-j` y las opciones de caché no cambian
el código. Las propiedades de proyecto de la biblioteca (por ejemplo `PanicProfile`) ya están en su bitcode, igual
que en su `.lib`.

## 6. Validación del consumidor

Para cada referencia estática del grafo de proyectos, en orden del grafo y sin repetir la misma `.lib`:

1. Sin sidecar: enlace nativo, sin mensaje.
2. Sidecar ilegible o corrupto: respaldo nativo.
3. Identidad de compatibilidad distinta a la del consumidor: respaldo nativo.
4. Digest de biblioteca distinto al SHA-256 de la `.lib` referida: respaldo nativo.
5. Digest de metadata distinto al SHA-256 de la `.nsmeta` referida: respaldo nativo.
6. Una dependencia que el consumidor no referencia, o cuya `.nsmeta` tiene otro SHA-256: respaldo nativo. Un
   cambio real de dependencia normalmente ya falla antes, por la huella ABI del cierre binario; esta revisión cubre
   lo que esa huella no cubra.

Cada respaldo con sidecar presente se informa como `thinlto-library-fallback: <archivo>: <motivo>` en la salida
del comando. Un respaldo nunca es un error de compilación.

## 7. Consumo

- Las unidades aceptadas se agregan al backend ThinLTO después de las unidades del proyecto, con identificadores
  `nativesharp-thinlto-library-<k>-unit-<j>`. Comparten índice combinado, imports, caché de backends y resoluciones
  de símbolos: la primera definición prevalece. Desde el punto 50 sólo quedan visibles los símbolos que piden los
  objetos nativos del enlace (entrada o exports, runtime y `.lib` no consumidas); el resto se internaliza, cada
  función y dato tiene su propia sección COMDAT y el enlace pasa `/opt:ref`.
- Los objetos de esas unidades reciben rangos de procedimientos CodeView y verificación COFF como cualquier objeto
  ThinLTO.
- Los objetos de cada biblioteca se empaquetan en un archivo COFF que reemplaza a la `.lib` original en la misma
  posición de la lista de enlace. Conservar un archivo, y no pasar los objetos sueltos, mantiene la semántica de
  extracción bajo demanda: un miembro que nada referencia no entra al binario.
- El archivo de reemplazo vive en `%TEMP%\nativesharp-thinlto-libraries\<digest>\<nombre original>.lib`, con
  `<digest>` = 16 hexadecimales del SHA-256 del nombre de la biblioteca y del tamaño y digest de cada objeto. NSLink
  escribe la ruta de cada archivo en la tabla de módulos del PDB y en el hash de su GUID; una ruta direccionada por
  contenido mantiene reproducibles los builds `-g`. El archivo se escribe como `.partial` y se renombra; entradas sin
  uso por más de una semana se podan.
- La salida informa `thinlto-libraries: <n>` con el número de bibliotecas consumidas.

## 8. Implementación (punto 48)

| Pieza | Archivo |
|---|---|
| Contenido y códec v2 | `ThinLtoLibrarySidecar.h`, `ThinLtoLibrarySidecarCodec.h/.cpp` |
| Bitcode, identidad y unidades | `ThinLtoBackend` (`unitBitcode`, `compatibilityIdentity`, `appendBitcodeUnit`) |
| Generador | `ProjectOopObjectGenerator` (`thinLtoRuns`, `thinLtoCompatibility`, captura y unidades de biblioteca) |
| Validación, archivo de reemplazo y escritura | `src/Driver/ThinLtoLibraryLinking.h/.cpp` |
| Publicación transaccional | `ArtifactPublisher::addStaticLibraryThinLtoSidecar` |
| Salida requerida del bundle | `addCompilationCodegenIdentity` |
| Entrada del bundle del consumidor | `CompilationInputManifest` (sidecar junto a cada `.lib` estática) |
| Restauración de paquetes | `appendOptionalThinLtoSidecar` en `ProjectCommands.cpp` |
| Símbolos visibles del enlace final | `ThinLtoLibraryLinking::preservedSymbols`, `ThinLtoBackend::collectUndefinedSymbols` |
| Pruebas | `Item391ThinLtoTests`, `ProjectOopLoweringTests` (`thinlto-internalization`), `ProjectCommandsTests` |

## 9. Riesgos y decisiones

- Las funciones de una biblioteca que su NSMETA ya transporta como cuerpo inline siguen llegando como `weak_odr`
  al consumidor. Su copia en el consumidor prevalece por ser la primera definición; LLVM descarta la de la unidad de
  biblioteca y el enlace COMDAT resuelve cualquier duplicado.
- Un símbolo local de la biblioteca referido por una función importada se promueve con el sufijo de hash del
  módulo; el miembro correspondiente del archivo de reemplazo lo define y se extrae por esa referencia.
- Desde el punto 53 el sidecar es una entrada del bundle del consumidor: un sidecar cambiado, agregado o quitado
  vuelve a generar el producto, aunque la `.lib` sea la misma.
- El sidecar no se firma más allá del SHA-256 de integridad: protege contra corrupción y desajuste, no contra un
  productor malicioso, igual que la `.lib`.
