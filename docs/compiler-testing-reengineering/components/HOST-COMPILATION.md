# Host.Compilation — proyectos, fuentes, grafo, SDK e identidad de tipos

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo hallazgos marcados.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión congelada:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Universo:** 24 archivos, 4,255 líneas y 144,247 bytes.
- **Inventario:** 160 símbolos físicos, 52 contratos y 416 casos diseñados.
- **Validación realizada:** lectura completa de 12 headers y 12 implementaciones; índice AST de definiciones,
  catálogo de diagnósticos, límites, defaults, rutas, caches y segunda pasada independiente.
- **No realizado:** build, ejecución, baseline, cobertura, instalación, despliegue o modificación del producto.

## 2. Frontera del componente

Host.Compilation convierte descriptores de proyecto y SDK en objetos ordenados que el frontend puede consumir.
Sus responsabilidades son:

1. validar y evaluar globs de fuentes;
2. representar proyectos, fuentes, referencias, opciones y settings;
3. cargar XML de `.nsproj` y `.nssdk` con un subconjunto controlado;
4. resolver rutas, sidecars, SDK y dependencias;
5. construir y reescribir el grafo de proyectos de forma determinista;
6. agregar declaraciones parciales y conservar identidad de tipos/símbolos.

No analiza sintaxis `.ns`, no resuelve tipos de cuerpos ni emite LLVM. Entrega entradas normalizadas a
Host.Frontend y consume políticas puntuales de Host.Language y el pool de Host.Driver.

## 3. Patrones glob

`GlobPattern` normaliza `\` a `/`, divide segmentos y rechaza patrón vacío, ruta absoluta, segmentos `.`/`..` y
`**` incrustado. El matching es case-insensitive por byte y admite `?`, `*` dentro de segmento y `**` como segmento
completo para cero o más niveles. Las rutas se comparan en forma genérica y un patrón inválido nunca coincide.

La implementación usa memoización bidimensional, pero el recorrido es recursivo. Eso limita explosión combinatoria,
no el consumo de stack; se conserva como contradicción abierta con el lineamiento general.

## 4. Identidad de tipos y nombres de símbolo

`TypeIdentity` exige identificadores válidos para módulo, namespaces, tipos contenedores y nombre. La identidad
incluye módulo, namespace, anidamiento, nombre y aridad. `fullyQualifiedName` usa `.` entre namespace/tipo exterior,
`+` entre tipos anidados y acento grave para aridad; `key` antepone `module::`.

`symbolFragmentForIdentityKey` conserva ASCII alfanumérico/`_` y codifica todo otro byte como `$HH`. Es reversible,
independiente de locale y evita colisiones entre `.`, `:`, `+`, `_` y otros bytes. Los helpers de texto LLVM aplican
la regla de quoting de AsmWriter y devuelven referencia/prefijo con `@` y comillas cuando corresponde.

## 5. Agregación de tipos parciales

Cada parte conserva identidad, clase de declaración, accesibilidad, modificadores, base, ordinal, offset y path.
No se admiten partes después de finalizar, identidades inválidas, path vacío ni ubicación duplicada.

La finalización ordena por key/ordinal/offset, agrupa por identidad y exige:

- mismo kind en todas las partes;
- si hay más de una parte, todas deben ser `partial`;
- accesibilidad explícita no contradictoria, con default `internal`;
- a lo sumo una base coherente;
- combinación válida de `abstract`, `sealed` y `static`.

Un fallo limpia el resultado agregado; una finalización exitosa es idempotente.

## 6. Modelo de proyecto

`Project` conserva archivo, módulo, output, fuentes, referencias de proyecto, referencias binarias, SDK, perfil de
panic, handler, package y friend modules. Si package está vacío adopta el módulo. `LibraryReference` es un contrato
de `.lib`, metadata `.nsmeta` obligatoria y DLL runtime opcional.

`ProjectDumper` publica una representación estable con output `exe`, `static-library` o `shared-library`, fuentes
ordinales y referencias. No incluye todos los settings: es un dumper dirigido, no serialización round-trip.

## 7. Conjunto de fuentes

`ProjectSourceSet` fija el directorio absoluto normalizado. Cada source debe existir, ser regular, permanecer bajo
el proyecto y usar extensión exacta `.ns`. La deduplicación es case-insensitive sobre ruta relativa.

`finalize` exige al menos una fuente salvo `allowEmpty`, ordena por clave folded con desempate textual, asigna
ordinales desde cero y congela el conjunto. Agregar después falla; finalizar otra vez conserva el resultado.

## 8. Parser XML controlado

`SimpleXmlParser` acepta BOM UTF-8, whitespace, comentarios e instrucciones de procesamiento antes/después del root.
Exige un único root y profundidad máxima 128. Soporta elementos normales/self-closing, atributos con comillas simples
o dobles, texto, nombres XML limitados y entidades `amp`, `lt`, `gt`, `quot`, `apos`.

Rechaza atributos duplicados, cierres no coincidentes, contenido posterior, comentarios inválidos, entidades
desconocidas, DOCTYPE, CDATA y otras declaraciones. Conserva sólo el primer error con línea/columna.

`parseElement` es recursivo aunque acotado; también queda registrado como contradicción normativa sin corrección.

## 9. Esquema `.nsproj`

El root debe ser `Project` sin atributos ni texto; sólo acepta `PropertyGroup` e `ItemGroup`. Propiedades conocidas:
`Name`, `Package`, `OutputType`, `OptimizationLevel`, `Language`, `PanicProfile`, `PanicHandler`, `OutputPath`,
`LibraryReferenceRoot`, `CompilerJobs`, `GenericInstantiationDepth` y `GenericInstantiationLimit`.

Las propiedades únicas rechazan duplicados. `OutputType` admite exe, library/static library y shared library;
optimización es O0–O3; jobs es 1–1024; presupuestos genéricos están limitados por las constantes públicas.
`<Language>` es entrada histórica de compatibilidad: se lee y se ignora; no selecciona ni rechaza sintaxis.

## 10. Items de proyecto

`Compile` exige exactamente uno de `Include` o `Remove`; `Exclude` sólo acompaña a `Include`. Las listas separadas
por `;` no admiten elementos vacíos y cada patrón pasa por `GlobPattern`.

`ProjectReference`, `LibraryReference`, `SdkReference` y `Friend` tienen atributos cerrados, cuerpo vacío y un solo
path/identidad por item. Library permite metadata/runtime opcionales no vacíos. Friend debe ser identificador y se
deduplica case-insensitive.

## 11. Archivo de proyecto y cache de descriptor

El archivo debe existir, ser regular, usar extensión exacta `.nsproj` y no superar 4 MiB. La lectura verifica tamaño,
conteo exacto y EOF. El error XML se prefija con el path canónico.

`ProjectLoaderCache` comparte descriptors por path folded, protege mapa/estadísticas con mutex y distingue reads de
hits. Una carrera de carga conserva la primera entrada publicada y cuenta el segundo resultado como hit.

## 12. Descubrimiento de fuentes

Sin reglas `Compile`, el default es `**/*.ns`. La enumeración recursiva omite `bin`, `obj`, `.git` y `.nsc`, y
continúa ante permisos denegados. Include/Exclude/Remove determinan candidatos; las claves folded eliminan
duplicados. Los candidatos pasan después por `ProjectSourceSet`.

Un proyecto sin fuentes es válido sólo cuando tiene ProjectReferences; esto permite nodos agregadores.

## 13. Referencias de proyecto

Las referencias se resuelven desde el directorio del proyecto, deben existir, ser `.nsproj`, no duplicarse por ruta
folded y se ordenan determinísticamente. El loader de grafo canonicaliza de nuevo antes de descubrir dependencias.

## 14. Referencias binarias

La raíz efectiva sigue este orden:

1. `ProjectLoadOptions.libraryReferenceRoot`;
2. `ProjectBuildSettings.libraryReferenceRoot`;
3. path local al proyecto;
4. `libraryReferenceFallbackRoot` o `NS_LIB_ROOT`, sólo si el artefacto local no existe.

El Include debe resolver a `.lib`; metadata explícita o sidecar `.nsmeta` es obligatoria; runtime opcional debe ser
`.dll`. Todos deben existir como archivos regulares. La deduplicación usa el path de la library y el resultado se
ordena. `NS_LIB_ROOT` sólo se acepta absoluto y nunca reescribe un artefacto local existente.

## 15. SDK manifests

Un `.nssdk` existe, es regular, no excede 4 MiB y tiene root `NativeSharpSdk`, `Name` identificador y
`FormatVersion=1`. Las secciones únicas son `Mappings`, `Targets` y `Libraries`.

Mappings soporta Header, Type, Field y Define con atributos cerrados y claves únicas por categoría. Targets usa
arquitectura identificador y versiones numéricas de hasta ocho partes; normaliza ceros finales, exige min <= max,
prohíbe rangos solapados por arquitectura y ordena. Libraries exige nombre identificador y path relativo `.lib` sin
`..`, deduplica por nombre y ordena.

`SdkTargetSelector` devuelve el primer rango compatible o `NSC-SDK-003`. Al cargar proyecto, cada library declarada
por el SDK debe existir o produce `NSC-SDK-004`.

## 16. Grafo de proyectos

`ProjectGraphLoader` fuerza al menos un worker, permite inyectar loader y procesa cada frontera en paralelo. Ordena
frontiers y referencias antes de publicar resultados. Rechaza excepciones de workers, fallos de carga y módulos
duplicados con identidad Windows canónica.

La visita DFS detecta ciclos con cadena de módulos y deja dependencias antes del root. El root es siempre el último
proyecto y `rootProject()` devuelve null sólo para grafo vacío. El DFS vigente es recursivo sin límite explícito y se
registra en la contradicción de recursión.

## 17. Ownership de fuentes y preferencia de source

Después del orden topológico, una fuente descubierta por globs de varios proyectos conserva como owner al primer nodo
dependency-first; los otros conjuntos se reconstruyen sin esa fuente.

Para clientes directos sin cierre NSMETA del Driver, una library se sustituye por source sólo cuando stems de `.lib`
y `.nsmeta` coinciden exactamente con un módulo source diferente. Se preservan packages no ambiguos y se ordenan
ambas clases de referencia.

## 18. Mutaciones del grafo

`addLibraryReferences` agrega closure binaria al owner conocido, deduplica y ordena. `addPrecompiledProjects` crea
nodos sintéticos de metadata, verifica owner e identidad de módulo, deduplica paths, conecta peers y conserva root.

`replaceLibraryReferencesWithSourceProjects` valida owner/source, elimina sólo la metadata coincidente, agrega el
ProjectReference sin duplicarlo y reconstruye orden topológico. Rechaza ciclos, referencias desconocidas y proyectos
inaccesibles. Ninguna de estas operaciones modifica archivos en disco.

## 19. Dependencias

- `Host.Language`: identidad de módulo y semántica de panic.
- `Host.Driver`: `CompilationWorkerPool` para carga paralela del grafo.
- `Host.Frontend`: consumidor principal de proyectos, fuentes y orden.
- filesystem y streams estándar: canonicalización, enumeración y lectura.
- `SimpleXmlParser`: parser limitado compartido por proyecto y SDK.

## 20. Invariantes observables

1. Todas las colecciones publicadas tienen orden determinista.
2. Las comparaciones de paths/módulos que modelan Windows son case-insensitive.
3. Root queda al final y toda dependencia precede a su consumidor.
4. Una fuente tiene un solo owner semántico en el grafo normalizado.
5. Una referencia binaria siempre tiene metadata y tipos de extensión correctos.
6. Inputs históricos como `Language` no se convierten en gates semánticos.
7. Fallos no publican grafos, proyectos, manifests ni agregaciones parciales.
8. Límites de tamaño, profundidad y presupuestos se validan antes de reservar o recorrer de más.

## 21. Riesgos y hallazgos conservados

1. Globbing, visita del grafo y parsing XML usan recursión; sólo XML tiene límite explícito.
2. `GlobPattern` memoiza estados, pero patrones/rutas extremadamente profundos aún presionan el stack.
3. La lectura de project/SDK es cacheada por path, no por huella o timestamp; el contrato vigente asume inmutabilidad
   durante la vida del loader.
4. Comparaciones case-insensitive operan byte a byte con `tolower`, no con normalización Unicode.
5. El parser XML es deliberadamente incompleto y no debe probarse como parser XML general.
6. Los dumpers son diagnósticos parciales, no formatos persistentes round-trip.
7. `NS_LIB_ROOT` es fallback de artefactos, no autoridad sobre el proyecto.

No se corrigió ninguno de estos puntos en esta fase.

## 22. Estrategia futura de pruebas

Los parsers usarán strings y árboles mínimos con diagnósticos exactos por frontera; filesystem y cache usarán árboles
temporales controlados; el grafo inyectará loaders y workers deterministas; SDK y referencias usarán artefactos
mínimos con extensiones/sidecars. Los oráculos comprobarán objetos, orden, paths, ownership y diagnósticos, no el
número de `if`, helpers internos o archivos en que una futura refactorización coloque la lógica.

Cada contrato recibe nominal mínimo, representativo, prerequisito ausente, estructura inválida, límites, variantes,
determinismo y escala. La última matriz es `NIGHTLY`; los casos profundos caracterizan la recursión vigente.

## 23. Estado de cierre

Los 24 archivos y 160 símbolos están trazados a 52 contratos con 416 casos diseñados. `Host.Compilation` queda
`AUDITADO` y `RECONSTRUIBLE` para la revisión congelada. Esto no afirma ejecución ni cobertura.
