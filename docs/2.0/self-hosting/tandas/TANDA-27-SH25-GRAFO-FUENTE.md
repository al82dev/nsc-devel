# NativeSharp 2.0 self-hosting — tanda 27: grafo fuente determinista

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `DIRECTED-PASSED` — baseline acumulado pendiente |
| Gate | `SH25` |
| Rama | `development-claude` |
| Commit de entrada | `9aa5e8680` |
| Commit de cierre | este commit (`selfhost(tanda-27): expand the deterministic source graph`) |
| Fecha | 2026-09-20 |
| Baseline | acumulado final pendiente; corrida inicial detenida por cambio de metodología |

> **Este expediente no declara cerrado el baseline acumulado.** El código compila y SH25 dirigido
> pasa; la barrida completa se ejecutará cuando terminen las tandas entregadas por Claude.

## 2. Objetivo, alcance y exclusiones

SH25 sustituye la selección de **un** archivo por el **grafo fuente completo y determinista**:
expansión de patrones `Compile Include`, expansión transitiva de `ProjectReference`, orden canónico
y diagnósticos de lexer/parser por unidad para las 22 unidades del proyecto raíz.

Hasta aquí, `ProjectLoader.FirstSourcePath` devolvía el primer `.ns` en orden alfabético de todo el
árbol y `ProjectLoader.Verify` **saltaba** cualquier include con `**` o `..`. Es decir: los globs no
se expandían, y `--emit-project-object` compilaba un archivo suelto.

**Fuera de alcance, y es la frontera con SH26:** esto NO unifica la semántica entre archivos. Cada
unidad raíz pasa por el lexer y parser existentes con su propio `FileId` y reporta sus propios
diagnósticos sintácticos. Las unidades de proyectos referenciados aparecen en el grafo, pero T27 no
promete que el parser cubra todavía toda su superficie. `SemanticAnalyzer.ErrorCount()` no aporta
errores en esta revisión; resolución de símbolos, namespaces y overloads entre archivos, junto con
diagnósticos semánticos reales, son trabajo explícito de SH26.

## 3. Diseño, métodos e invariantes

### 3.1 Capa nueva, superficie anterior intacta

`SourceGraph.ns` es **aditivo**. `ProjectModel`, `ProjectDumper`, `MetadataContract`,
`ProjectCache` y `ToolchainManifest` no cambian, porque `project-driver.contract.json` (SH09) fija
`expected_sources: 1` sobre el dump de includes crudos. Un grafo que reescribiera `ProjectModel`
habría movido ese número y roto un gate ya cerrado.

### 3.2 El orden canónico, que es lo que el gate mide

1. los proyectos referenciados aportan **antes** que el que los referencia (post-orden);
2. dentro de un proyecto, los includes aportan en **orden de declaración** — el `.nsproj` lista
   Basic, Source, Diagnostics, Lexer, … Driver, y esa estratificación es información, no casualidad;
3. dentro de un include, los archivos se ordenan por comparación ordinal de su ruta;
4. un archivo alcanzado por dos includes se conserva **una vez**, en su primera aparición.

Las reglas 2 y 3 juntas son lo que hace el volcado reproducible: ni el orden de enumeración del
sistema de archivos ni la configuración regional pueden mover una unidad.

La ordenación se escribe explícita (selección sobre el arreglo de nombres) **aunque `List<T>.Sort()`
exista**. La razón es auditable: el gate necesita que el orden sea demostrablemente ordinal, y la
semántica de comparación de `Sort()` sobre `string` no se verificó.

### 3.3 Rutas relativas al proyecto propietario

`SourceUnit` guarda su ruta relativa **al directorio de su propio proyecto**, no al del proyecto
raíz. Una biblioteca referenciada vive fuera del árbol raíz, así que una ruta relativa a la raíz
arrastraría el prefijo absoluto al volcado — y el volcado es justo lo que SH44 comparará entre dos
máquinas. Con la ruta relativa al propietario, `String.ns` se escribe `String.ns` en cualquier raíz.

### 3.4 Un include vacío es un error

`IncludeExpander` falla si un patrón no casa con ningún archivo, en vez de expandir a nada. Un
include que silenciosamente no aporta es como una compilación pierde un archivo sin que nadie lo
note.

### 3.5 Ciclos y profundidad

`SourceGraphBuilder` lleva una pila de proyectos activos y diagnostica el ciclo con la ruta
normalizada. El límite de anidamiento es 32.

## 4. Archivos y ownership

| Archivo | Acción | Responsabilidad |
|---|---|---|
| `selfhost/nsc/Projects/SourceGraph.ns` | crear | `SourceUnit`, `SourceGraph`, `IncludeExpander`, `SourceGraphBuilder`, `SourceGraphDumper` |
| `selfhost/nsc/Driver/Program.ns` | modificar | `--dump-source-graph`, `--check-project`, ayuda |
| `selfhost/contracts/source-graph.contract.json` | crear | proyectos, orden y 22 unidades raíz |
| `selfhost/bootstrap/verify_source_graph.py` | crear | identidad entre O0–O3, orden canónico, resumen |
| `cmake/VerifyNativeSharpSourceGraph.cmake` | crear | matriz O0–O3 |
| `CMakeLists.txt` | modificar | `nsc.selfhost.source-graph` |

## 5. Contrato y cobertura

`nativesharp-selfhost-source-graph-contract` v1 fija cinco proyectos en post-orden —
`NativeSharpCore`, `NativeSharpCoreCollectionsGeneric`, `NativeSharpCoreIO`, `NativeSharpLLVM_C`,
`NativeSharpCompilerNext` — y la lista exacta de unidades del proyecto raíz.

**Un número que cambia y conviene no heredar mal:** el roadmap habla de **21** archivos de
`nsc-next`. Esta tanda añade `Projects/SourceGraph.ns`, así que a partir de aquí el proyecto raíz
tiene **22** unidades, y el contrato dice 22. `Projects/ProjectSystem.ns` precede a
`Projects/SourceGraph.ns` por orden ordinal dentro del mismo include.

El contrato **no fija el total de unidades**, porque incluye las de las bibliotecas. El verificador
comprueba que la cabecera `units=` coincide con las líneas `unit` emitidas y filtra por propietario
para la lista exacta. `--check-project` informa por separado `graph-units`, `checked` y `failed`: el
primero cubre el grafo completo y `checked` debe coincidir con las 22 unidades raíz. Un número
inventado en un contrato es peor que ninguno.

`Core` es referenciado por tres proyectos; la deduplicación garantiza que aporta **una** vez y en
primer lugar.

## 6. Validación dirigida

Comprobaciones realizadas:

- Sintaxis del verificador Python y validez del JSON del contrato: aprobadas.
- Las APIs usadas se verificaron **leyendo** su declaración: `String.LastIndexOf(string, string)`,
  `List<T>.RemoveAt`, `Directory.GetFileCount/GetFiles/GetDirectoryCount/GetDirectories`,
  `Path.Combine/GetDirectoryName/GetFullPath`, `File.Exists`, y las sobrecargas de
  `StringBuilder.Append` (string, int, char) por su uso existente en `ProjectSystem.ns` y
  `SourceBuffer.ns`.
- **Un error encontrado así:** la primera versión llamaba `String.TrimEnd(head, '/')`. Esa
  sobrecarga **no existe** — `TrimEnd` sólo toma el valor. Se sustituyó por
  `TrimTrailingSeparators`, escrito con `Substring` e indexación.

La primera ejecución dirigida encontró correctamente 92 unidades y confirmó que las 22 unidades
raíz tienen cero errores de lexer/parser, pero falló porque la implementación inicial exigía también
cero errores a todas las unidades referenciadas. La medición posterior corrigió el diagnóstico: las
30 unidades fallidas tienen lexer verde y parser rojo; no demuestran un fallo de resolución
multimódulo. `String.ns`, usado como control, pasa tanto `--dump-tokens` como `--dump-ast`. El gate
se corrigió para separar el tamaño del grafo de las unidades raíz cuya cobertura sintáctica ya está
contratada.

Matriz diagnóstica entregada a SH26 (`tokens=0`, `ast=1` en las 30 filas):

| Proyecto | Unidades y conteo de errores del parser |
|---|---|
| NativeSharp.Core | `BinaryReader.ns` 3; `ByteBuffer.ns` 4; `CString.ns` 4; `Float32x2.ns` 3; `Float32x3.ns` 3; `Float32x4.ns` 8; `Float32x8.ns` 3; `Float64x4.ns` 3; `Half.ns` 1; `Int32x4.ns` 5; `Int32x8.ns` 3; `Int64x2.ns` 3; `Int64x4.ns` 3; `Span.ns` 6; `StringView.ns` 3 |
| NativeSharp.Core.Collections.Generic | `Algorithm.ns` 12; `Dictionary.ns` 4; `HashSet.ns` 4; `HashSetEnumerator.ns` 4; `LinkedList.ns` 4; `LinkedListNode.ns` 4; `List.ns` 6; `PriorityQueue.ns` 4; `Queue.ns` 4; `SortedDictionary.ns` 4; `SortedList.ns` 4; `SortedSet.ns` 4; `SortedSetEnumerator.ns` 4; `Stack.ns` 4 |
| NativeSharp.LLVM.C | `LLVMApi.ns` 1727 |

En total son 1852 errores de parser. El AST se vuelca incluso con código de salida 1 y actualmente no
imprime el detalle de esos diagnósticos; SH26 debe añadir una superficie diagnóstica observable antes
de usar esa matriz como entrada de diseño semántico.

Repetición final: **PASSED** en 10.69 s. `nsc.selfhost.source-graph` construyó candidatos O0,
O1, O2 y O3; los cuatro volcados fueron idénticos, el orden coincidió con el contrato y cada perfil
cerró con `graph-units=92 checked=22 failed=0`. Ese resumen certifica lexer/parser, no semántica.

## 7. Baseline de cierre

En el flujo de integración con Claude se ejecuta compilación y gate dirigido por
tanda; el baseline completo acumulado se difiere expresamente hasta el cierre de todas sus tandas:

```bat
ctest -R nsc.selfhost.source-graph -V
```

La corrida `tanda-27-sh25-grafo-fuente-20260920` alcanzó la compilación e instalación de `nsc0` y la
construcción completa del árbol CTest LLD. Se detuvo antes de ejecutar matrices y antes de construir
el árbol NSLink al cambiar la metodología; por tanto no cuenta como baseline aprobado.

Comando reservado para el baseline acumulado final:

```bat
python nscbuilder.py --baseline --jobs 16 --prefix A:\native-sharp ^
  --build-dir A:\nsc-devel\selfhost\out\nsc0-build ^
  --test-build-lld A:\nsc-devel\selfhost\out\tests-lld ^
  --test-build-nslink A:\nsc-devel\selfhost\out\tests-nslink ^
  --llvm-root C:\clang-llvm --nslink A:\native-sharp\bin\nslink.exe ^
  --history-root A:\nsc-devel\selfhost\evidence\runs ^
  --work-root A:\nsc-devel\selfhost\out\work --temp-root A:\nsc-devel\selfhost\out\temp ^
  --write-root A:\ --run-id tanda-27-sh25-grafo-fuente-20260920 ^
  --build-id 2700000000000000
```

Números esperados: **460/460** por árbol (459 + `nsc.selfhost.source-graph`). Si se mueve algún
otro, la causa está en esta tanda y no en otra parte: sólo se tocaron los seis archivos de §4.

## 8. Riesgos, deuda y continuidad

- **`--emit-project-object` sigue compilando un solo archivo.** Esta tanda construye el grafo y
  recorre las unidades raíz con lexer/parser; conectarlo al backend es SH27, cuando el backend
  acepte módulos reales.
  `FirstSourcePath` se conserva intacta por eso.
- `IncludeExpander` sólo honra `ruta literal`, `dir/*.ext` y `dir/**/*.ext`. Cualquier otro patrón
  es un error explícito. Ningún `.nsproj` del árbol usa otra forma; si aparece, el error dice cuál.
- El orden canónico depende del orden de declaración de los includes. Reordenar el `.nsproj`
  cambia el volcado **a propósito**: es información del autor, no ruido.
- La ordenación por selección es O(n²) sobre los nombres de un directorio. Con directorios de
  decenas de archivos no se nota; si un día importa, se sustituye midiendo antes.

- [x] Compila.
- [x] `nsc.selfhost.source-graph` pasa O0–O3.
- [ ] Baseline completo acumulado en ambos árboles al terminar las tandas de Claude.
- [ ] Evidencia SH25 versionada.
- [ ] Promoción autoalojada completa; continúa bloqueada por SH26–SH41.
