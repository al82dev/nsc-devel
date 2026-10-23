# NSC — Auditoría y diseño de la compilación incremental

**Fecha:** 2026-10-22 (UTC 2026-10-23).
**Rama:** `development-claude-incremental-compilation`, creada por el usuario como copia de
`development-claude-cold-compilation`. Último commit previo al trabajo: `991eead84`.
**Alcance:** auditoría a fondo de qué reutiliza hoy `nsc` entre compilaciones, medición en los cuatro
niveles de optimización de NativeSharp (`-O0`, `-O1`, `-O2`, `-O3`), diseño de la compilación incremental e
implementación de las partes que no requieren cambiar pruebas ni contratos.
**Convención:** HECHO = verificado en código, en disco o con una medición propia que se puede repetir.
INFERENCIA = conclusión razonada que todavía no se midió. NO VERIFICADO = no se ejecutó.

---

## 0. Resumen ejecutivo

1. **Hoy `nsc` no tiene compilación incremental del frontend (HECHO).** Cualquier cambio de un byte en
   cualquier fuente repite *todo* el frontend:
   - grafo del proyecto;
   - carga y decodificación de NSMETA de las referencias;
   - parseo;
   - genéricos;
   - nombres, miembros y cuerpos;
   - construcción y layout;
   - HIR y MIR;
   - escritura de NSMETA.

   Sólo hay tres reutilizaciones reales:
   - el **bundle de compilación**, que devuelve las salidas cuando la entrada es idéntica byte a byte;
   - la **caché de objetos LLVM por unidad de código**, indexada por identidad MIR;
   - la **caché del archivo estático**, cuando todos los objetos coinciden.
2. **La caché LLVM sólo sobrevive a ediciones de cuerpo o de comentarios (HECHO).** Agregar un método
   privado, un método público o un campo privado invalida el 100 % de las unidades en los cuatro niveles. Hay
   dos causas:
   - la identidad MIR incluye un digest común con *todas* las tablas del modelo;
   - incluye ordinales globales de funciones y miembros, que se recorren al insertar una declaración.
3. **Un build incremental puede producir un `.lib` distinto del build limpio de las mismas fuentes (HECHO).**
   En `NativeSharp.Math.Functions`, la edición de un solo cuerpo dio objetos distintos en O0, O1, O2 y O3. La
   causa es el plan de partición `CacheAwareBalanced`: se restaura del historial de la caché, así que el reparto
   de funciones entre unidades depende de compilaciones anteriores. En O1–O3 esto cambia además el código
   optimizado, porque el inlining sólo ocurre dentro de una unidad.
   - **Esto contradice la equivalencia estricta.**
   - **No lo cambié:** una prueba existente (`ProjectOopLoweringTests`) fija ese diseño, y las pruebas no se
     modifican sin autorización (sección 9).
4. **El `.lib` cambiaba byte a byte entre dos builds de la misma entrada (HECHO).** Los nombres de los miembros
   del archivo COFF llevaban la ruta temporal `…\Temp\nativesharp-codegen-<reloj>-0\program-N.obj`.
   - **Corregido en esta rama.** Los miembros ahora son `program-N.obj`.
5. **Un build sin cambios (acierto de bundle) pagaba la materialización completa de NSMETA de todas las
   referencias antes de consultar el bundle (HECHO).**
   - **Corregido en esta rama:** la consulta ocurre antes de materializar y un acierto verificado la omite.
6. **Reutilización de objetos por IR previo a la optimización (implementada en esta rama).**
   - **Qué hace:** cuando falla la identidad MIR, se baja la unidad a LLVM IR (barato) y se busca un objeto
     compilado desde IR idéntico. Sólo se optimiza y emite si no existe.
   - **Por qué es sólido:** optimizar y emitir es una función determinista del IR, del target y del nivel, así
     que conserva la equivalencia estricta.
   - **Dónde ayuda:** es justo el costo que domina en O2/O3 (10.5 s de 18.4 s en `Math.Functions`).
   - **Medición:** los resultados por nivel están en la sección 8.
7. **Aun con LLVM perfecto, el piso de un build incremental lo fija el frontend completo (HECHO medido).**
   En `Math.Functions` son ~7–9 s de 11–19 s. La compilación incremental real exige reutilizar el frontend
   por declaración y por cuerpo. Eso queda diseñado en la sección 7, con los prerrequisitos que faltan.

---

## 1. SCOPE LOCK

| Campo | Valor |
|---|---|
| Objetivo autorizado | Auditar y analizar a profundidad la compilación incremental de `nsc` e implementarla en la rama `development-claude-incremental-compilation`, sin tocar la rama de compilación en frío. |
| Naturaleza | Optimización con equivalencia estricta, más correcciones de determinismo en los artefactos. Las partes que cambian contratos o pruebas se proponen y no se aplican (sección 9). |
| Comportamiento idéntico | Semántica del lenguaje, diagnósticos, NSMETA, encabezado `.h`, contenido de cada objeto y orden de los miembros del `.lib`, salida normal del driver. La salida `--time` sólo gana el campo `ir-reuse=` por unidad. |
| Último estado conocido | `991eead84`. Su build O3 (`tc-base`) es el oráculo de las comparaciones A/B. |
| Rango evaluado | `991eead84..HEAD` de esta rama. |
| Archivos modificables | `src/Linker/LldLinker.cpp`, `src/LLVM/ProjectOopObjectGenerator.cpp`, `include/nativesharp/LLVM/{CodeUnitTelemetry.h,CodeUnitIrReuse.h,LlvmOptimizer.h}`, `src/Driver/{DriverExecution.cpp,PrecompiledModuleConfiguration.cpp}`, `src/Compilation/PrecompiledProjectPreparation.cpp`, `include/nativesharp/Compilation/PrecompiledProjectPreparation.h` y este documento. |
| Protegidos | `tests/**`, `CMakeLists.txt`, fixtures, `.nsproj`, bibliotecas consumidoras, formato NSMETA, formato del bundle, rama `development-claude-cold-compilation`. |
| Pruebas que deben pasar sin cambios | `nsc_project_oop_lowering_tests`, `nsc_compilation_artifact_cache_tests`, `nsc_namespace_function_llvm_tests`, con el mismo resultado que en `991eead84` (sección 10). |
| Prohibido | Modificar pruebas. Escribir en `C:`. Borrar cachés o archivos sin autorización. Cambiar la política de partición mientras exista la prueba que la fija. Suponer un nivel de optimización. |
| Terminación | Documento completo, cambios compilados con el nivel de host configurado por el usuario (leído de `CMakeCache.txt`: `NSC_HOST_OPTIMIZATION=O3`, `Release`), pruebas comparadas contra la base, A/B medido en O0–O3, un commit por hipótesis. |

---

## 2. Entorno y método

### 2.1 Compiladores

- **`nsc` instalado.** `C:\native-sharp\bin\nsc.exe`, compilado con clang-cl, `Release`, `NSC_HOST_OPTIMIZATION=O3`.
  Leído de `A:\nsc-devel\build-clang\CMakeCache.txt`; sus banderas terminan en `/clang:-O3` (HECHO).
- **`tc-base` (oráculo).** Build limpio de `991eead84` en un worktree separado,
  `A:\nsc-devel-baseline-991eead84\build-baseline`. Misma configuración que el instalado: clang-cl 23.1, Release,
  host O3, LLVM 23.1 estático.
- **`tc-new`.** Build de esta rama en `A:\nsc-devel\build-incremental` con la misma configuración.
- **Ambos se copiaron** a `A:\temp\nsc-incremental-bench\tc-*` con el layout instalado
  (`bin\nsc.exe` y `lib\nativesharp\runtime\NativeSharp.Runtime.Win32.lib`), sin tocar `C:`.
- **HECHO, ambiente.** Ejecutables lanzados desde `A:\nsc-devel\build-incremental\…` reciben
  `Access is denied` al crear directorios. La copia del mismo `.exe` en otra carpeta funciona. La causa no está
  verificada; probablemente sea una política de Windows (Controlled Folder Access o antivirus) sobre esa carpeta.
  Por eso todas las mediciones y pruebas corren desde copias.

### 2.2 Banco

- **Fuentes.** `A:\temp\nsc-incremental-bench\nslibs`, copia de `A:\nativesharp-libraries` sin `.git`. Las
  ediciones de los escenarios se hacen sobre la copia y se revierten byte a byte.
- **Cadena.** Las 33 bibliotecas del catálogo en orden topológico, con `--library-reference-root` hacia la salida
  del mismo nivel.
  - `NativeForms` falla en los tres compiladores con
    `OleDropTarget.ns:220:30: error: void cannot be used as an allocation element type`.
  - Es una regresión aparte, sección 6, H12.
- **Bibliotecas de los escenarios:**
  - `NativeSharp.Logging`: 61 archivos, poca carga genérica.
  - `NativeSharp.Math.Functions`: la más pesada del catálogo, con 11 referencias y un pico de ~4.6 GB de
    memoria.
- **Escenarios** (archivo y edición en `nsc_scenarios.py`):

  | Id | Edición | Qué representa |
  |---|---|---|
  | s0-prime | ninguna, caché vacía | build en frío que llena la caché |
  | s1-nochange | ninguna | rebuild sin cambios |
  | s1b-touch | reescribe los mismos bytes | cambio de mtime sin cambio de contenido |
  | s5-comment | texto de un comentario | cambio sin semántica |
  | s2-body | cuerpo de un método (`<= 0` → `< 1`, `Cos(x)` → `Cos(x + 0.0f)`) | edición de cuerpo |
  | s3-private-method | agrega un método privado | superficie privada |
  | s4-public-method | agrega un método público | superficie pública |
  | s6-private-field | agrega un campo privado a `Logger` (sólo Logging) | layout |

- **Por escenario se hacen tres builds:**
  1. **incremental:** caché caliente del mismo nivel;
  2. **referencia:** caché nueva y vacía, es decir, build limpio;
  3. **revert:** se restaura la fuente y se compila con la caché caliente.
- **Equivalencia.** Se compara el SHA-256 de `.nsmeta` y `.h`, y un digest del `.lib` que recorre los miembros
  del archivo e ignora sólo la tabla de nombres largos (`//`).
  - Esa tabla contenía la ruta temporal (H2), así que compararla byte a byte daba "distinto" incluso con dos
    builds en frío idénticos.
  - Con la corrección H2, el `.lib` de `tc-new` también es comparable byte a byte.
- **Ninguna caché se borra.** Cada corrida usa su propia etiqueta de caché.

### 2.3 Limitaciones de medición

- **HECHO.** El tiempo tiene ruido alto en esta máquina.
  - Ejemplo: un acierto de bundle en `Math.Functions` midió 0.9 s y 2.9 s en corridas consecutivas.
  - La **primera** ejecución de un `.exe` recién compilado tarda ~10 s más. Probablemente sea un escaneo de
    antivirus (NO VERIFICADO).
  - Por eso las conclusiones se apoyan en conteos (hits, unidades, fases que corren o no) y en diferencias
    grandes, no en milisegundos aislados.
- **HECHO.** La primera tanda O3 de `Math.Functions` con el `nsc` instalado coincidió con un build de `nsc` en
  paralelo. Sus tiempos no se usan; la tanda A/B de la sección 8 se repitió sin carga.

---

## 3. Inventario de los mecanismos de reutilización existentes (HECHO, en código)

### 3.1 Bundle de compilación

- **Código.** `CompilationInputManifest` + `CompilationBundleCache`; se consulta en
  `driver::configurePrecompiledModules`.
- **Identidad.** `nsc-compilation-bundle-v1`, la versión del compilador, los campos de identidad y, por cada
  archivo de entrada, su ruta lógica, tamaño y SHA-256. Los campos de identidad son: producto, nombre de salida,
  nivel de optimización, depuración, LTO, prefijos de depuración, enlace y subsistema. Los archivos de entrada
  son las fuentes, los `.nsproj` y los NSMETA/LIB referenciados.
- **Es por contenido, no por mtime (HECHO medido: s1b-touch acierta).**
- **Acierto.** Restaura `.lib`, `.h`, `.nsmeta` y `.nsproducts` desde
  `<caché>/compilation-bundles/<id>/` y vuelve a imprimir el reporte guardado.
- **Limitación 1, H8.** El reporte reimpreso incluye las líneas `code-unit … miss-reason=` del build que
  produjo el bundle. En un acierto parece que hubo misses en frío.
- **Limitación 2, H1.** Antes de esta rama la identidad se calculaba **después** de materializar todos los
  módulos precompilados de las referencias. El acierto pagaba esa materialización.
- **Cualquier cambio de un byte en cualquier fuente es un miss total del bundle.** Ahí no hay granularidad.

### 3.2 Caché de objetos LLVM por unidad

- **Código.** `ProjectOopObjectGenerator`, `ProjectOopMirCacheIdentity`, `CompilationArtifactCache`.
- **Unidades.** El programa se parte en *k* unidades más una unidad `globals`.
  - En O0, *k* = min(workers, 8).
  - En O1–O3, *k* = workers. En esta máquina dio 15 (HECHO, `code-units: 15`).
- **Clave pre-LLVM (`inputObjectKey`)** = SHA-256 de:
  - identidad de codegen del compilador, versión de LLVM, target, política, nivel, pipeline y depuración;
  - **identidad MIR indexada**, que a su vez contiene:
    - un **digest común**: todos los layouts de struct y class, campos estáticos, tablas de despacho, bindings
      de interfaz, especializaciones genéricas e inicializadores de campo de **todo** el programa;
    - por cada función declarada en la unidad, su **ordinal global** y el digest de su definición o
      declaración. Ese digest incluye `memberOrdinal`, `declaringTypeOrdinal`, `localOrdinals` y los ordinales
      de los nodos.
- **Clave post-LLVM (`objectKey`).** Sobre el IR **ya optimizado**. Sólo se usa con depuración completa o
  ThinLTO.
- **Acierto.** Copia el `.obj` (≈8 ms por unidad en Logging) y lo verifica como COFF (≈6 ms).

### 3.3 Plan de partición `CacheAwareBalanced`

- **Clave del plan.** `persistentPlanKey` = SHA-256 de los nombres decorados y los ordinales de las funciones
  alcanzables. Excluye los cuerpos, así que una edición de cuerpo conserva la clave.
- **Con plan.** Se restaura el reparto del build anterior.
- **Sin plan.** Se usa *Balanced*: longest-processing-time por costo estimado del cuerpo.
- **Consecuencias:**
  - agregar o quitar una función cambia la clave y redistribuye **todas** las funciones;
  - una edición de cuerpo conserva el reparto anterior, mientras que un build limpio repartiría según los
    costos nuevos. Eso es H3.

### 3.4 Caché del archivo estático y del runtime

- **Archivo estático.** `restoreStaticArchive` usa como clave el tamaño y SHA-256 de cada objeto. Sólo acierta
  si **todos** los objetos coinciden.
- **Objeto del runtime.** Va por separado. En estas bibliotecas el runtime es externo
  (`disabled-or-external`).

### 3.5 Cachés del frontend

- **Corpus de parseo y superficies precompiladas.** Usan `setTemplateCacheRoot` de `ProjectCompilation`. Se
  corrigieron en la rama de frío para publicar árboles prístinos.
  - El corpus sólo acierta con **todas** las fuentes idénticas.
  - Las superficies precompiladas sí se reutilizan por módulo referenciado.
- **C1 (registros de plantillas), C3 (clones especializados) y la caché MIR por callable.** Están
  **desconectadas** desde la rama de frío (commits `cf0a94725` y `82615f62f`), por dos razones:
  - eran incorrectas (C3 omitía `rewriteSpecializedSyntax` en un acierto; C1 reusaba dependencias de otra
    compilación);
  - la clave MIR es el volcado HIR **completo** del programa, así que nunca podía acertar con una edición. Con
    fuentes idénticas ya responde el bundle.
- **No existe ninguna reutilización por archivo, declaración o cuerpo** en genéricos, nombres, miembros,
  cuerpos, construcción, layout, HIR, MIR ni NSMETA.

### 3.6 Trabajo fijo fuera de las fases medidas (`other`)

`timing total_ms` se imprime al destruir el `DriverExecutionScope`. Entre fases hay trabajo que `--time` no
etiqueta:

- la poda de la caché (`artifactCache.prune`) **al inicio de cada invocación**: recorre todo el directorio de
  la caché;
- la publicación del bundle, que copia las salidas a `compilation-bundles`;
- la destrucción de los modelos del frontend, con un pico de 4.6 GB en `Math.Functions`;
- los huecos entre fases.

Ese `other` vale 1.3–3.7 s en `Math.Functions`. **Su composición exacta está NO VERIFICADA:** no hay
instrumentación para separarlo.

---

## 4. Mediciones

{{TABLES}}

---

## 5. Qué se reutiliza y qué se recalcula, por escenario

| Escenario | Bundle | Frontend (todas las fases) | Unidades LLVM | NSMETA / `.h` | Archivo `.lib` |
|---|---|---|---|---|---|
| sin cambios / touch | **acierto** | no corre | no corre | restaurados | restaurado |
| comentario | miss | **completo** | todas aciertan: el MIR es idéntico | recalculados (mismos bytes) | se vuelve a armar; con H2 corregido coincide con el anterior |
| cuerpo | miss | **completo** | aciertan todas menos la unidad del método (MIR) | recalculados | se vuelve a armar |
| método privado | miss | **completo** | **0 % de aciertos MIR**, causas abajo | recalculados | nuevo |
| método público | miss | **completo** | **0 % MIR** | recalculados (cambia la superficie) | nuevo |
| campo privado | miss | **completo** | **0 % MIR** | recalculados | nuevo |

### 5.1 Cadena causal del 0 % de aciertos al agregar un método o un campo (HECHO, en código)

1. Una declaración nueva recorre `memberOrdinal` de todos los miembros posteriores. Un método nuevo además
   recorre los ordinales de función de `model.functions()`.
2. El digest común serializa `slotMemberOrdinals`, los `memberOrdinal` de los campos estáticos, los
   inicializadores, las tablas de interfaz y los layouts de **todo** el programa. Cambia → cambia la clave de
   **todas** las unidades, incluida `globals`.
3. Aunque el digest común no cambiara, el digest por función incluye el ordinal global y los `memberOrdinal`
   de cada nodo de llamada. Cambian en cada función que llama a un miembro declarado después del insertado.
4. Con un método nuevo también cambia `persistentPlanKey`: el plan no se restaura, Balanced redistribuye todas
   las funciones y cada unidad recibe un conjunto distinto.

### 5.2 Por qué el comentario sí acierta en LLVM y aun así cuesta casi un build completo

- El MIR no contiene posiciones sin `-g`, así que todas las unidades aciertan (HECHO: 9/9 en O0 y 16/16 en
  O1–O3).
- Pero el bundle falla (cambiaron bytes de la fuente), y no hay ninguna reutilización del frontend. En
  `Math.Functions` eso deja ~8–12 s de frontend y trabajo fijo.

---

## 6. Hallazgos

| Id | Clasificación | Hallazgo | Evidencia |
|---|---|---|---|
| H1 | HECHO, corregido | El acierto de bundle materializaba todos los módulos precompilados antes de consultar el bundle | `configurePrecompiledModules`: `preparePrecompiledProject` antes de `calculateIdentity`; s1 en `Math.Functions` con `precompiled-metadata` 743 ms y pico de 108 MB |
| H2 | HECHO, corregido | El `.lib` difiere byte a byte entre builds idénticos: nombres de miembro con ruta temporal y reloj | `nsc_compare.py`: payloads idénticos, tabla `//` distinta (`nativesharp-codegen-2358…` contra `2361…`) |
| H3 | HECHO, **no corregido (requiere autorización)** | La salida incremental depende del historial de la caché: el plan `CacheAwareBalanced` restaurado no coincide con el reparto de un build limpio | s2-body en `Math.Functions`: `equal False` en O0, O1, O2 y O3; objetos `program-1..8` con tamaños distintos y `program-0` (globals) idéntico |
| H4 | HECHO | El número de unidades depende de los hilos de la máquina en O1–O3 (`partitionCount = workerCount`). El `.lib`, y en O1–O3 el código optimizado (inlining sólo dentro de una unidad), dependen de la máquina | `ProjectOopObjectGenerator::generate`; `code-units: 15` aquí |
| H5 | HECHO | La identidad MIR usa ordinales globales; cualquier inserción invalida casi todo | sección 5.1 |
| H6 | HECHO | El digest común cubre todo el modelo; un campo en una clase invalida todas las unidades | s6-private-field: 0/9 y 0/16 |
| H7 | HECHO | La clave del plan incluye el conjunto de funciones; agregar una función redistribuye todo | `persistentPlanKey` |
| H8 | HECHO | En un acierto de bundle, el reporte `--time` reimprime las líneas `code-unit` del build original (`miss-reason=cold`) | log de s1-nochange |
| H9 | HECHO | Cada acierto LLVM paga copia y verificación COFF (≈14 ms por unidad), y el `.lib` se vuelve a armar si cambió cualquier objeto | líneas `code-unit` de s5-comment |
| H10 | HECHO | Sin reutilización del frontend; pico de 4.6–5.2 GB en `Math.Functions` (`method-bodies` sube de 0.7 a 2.6 GB) | `peak_working_set_bytes` por fase |
| H11 | HECHO, causa NO VERIFICADA | `Math.Core`: ~1 s de `oop-hir` con 8 ms de `method-bodies` | tabla en frío, sección 4 |
| H12 | HECHO, causa NO VERIFICADA | Regresión: `new void*[7]` y `stackalloc void*[n]` rechazados con `void cannot be used as an allocation element type` (NativeForms y selfhost) | logs de la cadena; también con `tc-base` |
| H13 | HECHO | Las pruebas `nsc_project_oop_lowering_tests`, `nsc_namespace_function_llvm_tests` y `nsc_compilation_artifact_cache_tests` **no enlazan** con el `CMakeLists.txt` actual: faltan `nsc_basic` y `nsc_driver` en su línea de enlace (`kCompilerBuildId`, `CompilationGraphScheduler::clear`, `writeFileTransactionally`) | mismo error en `991eead84` y en esta rama |
| H14 | HECHO | Con el ambiente actual, 26 de 629 verificaciones de `nsc_project_oop_lowering_tests` fallan **igual** en `991eead84` y en esta rama: falta `C:/Program Files (x86)/NativeSharp/sdk/win64/import-libraries` y fallan los fixtures de agregados | sección 10 |

---

## 7. Diseño de la compilación incremental

### 7.1 Principios

- **Equivalencia estricta.** La salida incremental debe ser **byte a byte** la de un build limpio con el mismo
  compilador, en cualquier nivel `-O0` a `-O3` y con cualquier nivel de host. Por eso toda reutilización se
  apoya en claves de **contenido** que cubren exactamente las entradas de la función que se memoiza. Nunca se
  apoya en el historial.
- **El nivel de optimización y el `nsc` que corre son parte de cada clave.** Están en
  `inputObjectKey`/`objectKey` (nivel, pipeline y codegen identity del compilador) y en la identidad del bundle.
  Nada asume O2 ni O3: el diseño es el mismo en los cuatro niveles. Lo que cambia por nivel es **dónde** está
  el costo:
  - en O0 domina el frontend;
  - en O2/O3, LLVM (sección 4).
- **El cambio más pequeño invalida lo mínimo.** Las claves usan nombres estables (nombres decorados, claves de
  tipo), no ordinales.

### 7.2 Capas

| Capa | Qué reutiliza | Clave | Estado |
|---|---|---|---|
| L0 | Salidas completas (bundle) | Identidad del manifiesto | Existía; el acierto ya no materializa NSMETA (**implementado**, H1) |
| L1 | Archivo `.lib` determinista | Nombres de miembro estables | **Implementado** (H2) |
| L2 | Objeto por unidad desde IR idéntico | SHA-256 del IR previo a la optimización + target + nivel + pipeline + identidad de codegen + inlining obligatorio | **Implementado** |
| L3 | Partición determinista y estable | Función pura del MIR: cubetas por hash del nombre decorado, con Balanced cuando alguna queda vacía | **Diseñado**; un prototipo sólo pasó la revisión de sintaxis local (no se compiló con `nsc` ni se midió) y **no se aplicó** por la prueba que fija `CacheAwareBalanced` (sección 9) |
| L4 | Objeto por unidad sin bajar a IR | Identidad MIR **simbólica**: sin ordinales y con un digest común **por unidad** (sólo tipos, tablas y estáticos que la unidad referencia) | Diseñado; requiere auditar cada uso de ordinal en el lowering |
| L5 | Frontend por declaración y por cuerpo | Ver 7.4 | Diseñado; prerrequisitos en 7.5 |
| L6 | NSMETA de referencias decodificado | Snapshot binario mapeable, por SHA-256 del NSMETA | Diseñado |

### 7.3 L2 en detalle (implementado)

Flujo por unidad en `processObjectCodeUnit`:

1. Clave MIR → acierto: restaura (sin cambio).
2. Miss → baja la unidad a LLVM IR (≈5–7 ms por unidad en Logging) → configura el target.
3. `preOptimizationObjectKey`: imprime el IR, le antepone el dominio
   `NativeSharp.PreOptimizationIrObject.v1`, la bandera de inlining obligatorio, CPU y features, y la pasa por
   `objectKey`. Esta última agrega la identidad de codegen del compilador, la versión de LLVM, el triple, el data
   layout, la política, **el nivel**, el pipeline y la depuración.
4. Acierto verificado → `cacheHit = 1`, no se optimiza ni se emite.
5. Miss → optimiza, emite, guarda con la clave MIR **y** con la clave de IR.

**Solidez.**

- El objeto es una función determinista de esas entradas. La salida no cambia respecto de un build limpio,
  porque el objeto reusado es el que el build limpio habría emitido desde el mismo IR.
- El dominio evita choques con las claves post-optimización.
- Se desactiva donde la caché MIR también lo está (depuración completa, ThinLTO), porque ahí la clave
  post-optimización ya cubre el caso.

**Límites.**

- Los nombres derivados de ordinales de **tipo** (`__nativesharp_allocator_deallocate_<typeOrdinal>`,
  sufijos de estado estático) cambian el IR si se inserta un **tipo**. Insertar un método o un campo no los
  mueve.
- Con `CacheAwareBalanced`, agregar una función redistribuye todas las unidades (H7). Su IR cambia y L2 no
  ayuda en ese escenario hasta que se aplique L3.

### 7.4 L5: frontend incremental (diseño)

Unidad de reutilización: **la declaración** (tipo o miembro) y **el cuerpo** (callable).

1. **Huella de superficie de declaración por archivo.** SHA-256 de la secuencia de tokens de la superficie:
   - firmas;
   - modificadores;
   - tipos de campos;
   - atributos;
   - genéricos y restricciones;
   - el texto de las expresiones constantes y los inicializadores que la superficie expone.

   Excluye cuerpos, comentarios y espacios. La versión sin posiciones sólo aplica sin `-g` y cuando ninguna
   salida contiene rangos fuente; hay que verificar NSMETA.
2. **Superficie del proyecto.** La combinación ordenada de las superficies por archivo, más los digests de las
   referencias.
   - Si no cambia, los modelos `names`, `members`, `inheritance`, `interfaces`, `generic-constraints`,
     `virtual-dispatch`, `destruction` y `layout` son idénticos. Pueden restaurarse de un snapshot en lugar de
     reconstruirse.
   - Prerrequisito: serializarlos o mantenerlos vivos en un proceso servidor.
3. **Cuerpos.** Cada callable de la onda 0 se analiza en aislamiento (`ProjectCallableWaveScheduler`: *"may
   mutate only state owned by the returned isolated result"*).
   - Clave = superficie del proyecto + digest del cuerpo + ordinal de onda.
   - Si coincide, se reutiliza su `ProjectCallableAnalysisResult` y el reductor lo fusiona igual.
   - Las ondas siguientes (descubrimientos) dependen de las fusiones y se mantienen en frío.
4. **HIR y MIR por función.** Clave = clave del cuerpo + superficie del proyecto. Así se reconstruye
   correctamente la caché MIR por callable que hoy está desconectada, sin el volcado completo del programa.
5. **Genéricos.** Es la fase más cara (1.3–4.2 s en `Math.Functions`) y la más acoplada:
   - muta el AST en el mismo lugar;
   - sus especializaciones dependen de todo el programa.

   La reutilización segura exige, primero, que el pase deje de mutar el AST compartido (C3 falló justo por
   eso). Después, se memoiza cada especialización por (plantilla canónica, argumentos canónicos, superficie de
   las dependencias).
6. **NSMETA de salida.** Si la superficie pública no cambia, el NSMETA tampoco. Puede restaurarse por clave
   (1.3 s en `Math.Functions`).

### 7.5 Prerrequisitos de L5 (por qué no se implementa en esta tanda)

- **Serialización o persistencia de los modelos del frontend.** Hoy no existe; las únicas serializaciones
  (C1, C3, MIR) se desconectaron por incorrectas.
- **Un pase genérico que no mute el AST.**
- **Una auditoría de cada dato global** que lee el análisis de un cuerpo, para que la clave de superficie sea
  completa. Ejemplos: inferencia de efectos, escape e inlining.
- **Alternativa sin serializar: un proceso servidor (`nsc` residente)** que conserve los modelos en memoria
  entre builds e invalide por superficie. Es la vía de menor riesgo de corrección, pero también es un cambio
  de arquitectura del driver.

### 7.6 Ganancia esperada por capa (con números de la sección 4)

- **L2 + L3.**
  - Un método o campo nuevo en O2/O3 pasaría de compilar 15–16 unidades a ~1–2.
  - En `Math.Functions` son ~9–10 s menos de LLVM.
- **L5.**
  - En la edición de un cuerpo, el frontend bajaría de ~7–9 s a lo que cuesten el cuerpo, su HIR/MIR y el
    trabajo fijo.
  - Ese trabajo fijo incluye `precompiled-metadata` (0.5–0.9 s) y `other` (1.3–3.7 s), salvo que también se
    reduzcan.
- **L6 y el trabajo fijo (`other`)** son el piso de cualquier build y hay que instrumentarlos primero (H10).

---

## 8. Implementado en esta rama y resultados A/B

{{AB}}

---

## 9. Decisiones que requieren autorización

1. **Política de partición determinista (L3, corrige H3 y H7).**
   - **Qué la frena.** `ProjectOopLoweringTests` exige que la generación con caché publique el plan
     `CacheAwareBalanced` bajo `persistentPlanKey` y que ese plan reproduzca el reparto Balanced en frío
     (*"cold generation atomically publishes its private code-partition plan"*).
   - **Qué propongo.** Una política `DeterministicIncremental`: cubetas por hash del nombre decorado, con
     Balanced si alguna queda vacía.
     - Es función pura del MIR.
     - Mantiene el número de unidades.
     - Agregar una función no mueve ninguna otra.
   - **Qué requiere.** Modificar o retirar esa expectativa de la prueba. Según los lineamientos, eso es un
     cambio funcional que sólo el usuario puede autorizar.
   - **Además, desde H4:** para que el resultado no dependa de la máquina, el número de unidades en O1–O3 debe
     ser fijo o depender sólo del programa, no de los hilos. Eso cambia el código optimizado.
2. **Nombres de miembro del `.lib` (H2).**
   - Cambian bytes del artefacto emitido: la tabla de nombres largos.
   - El contenido de cada objeto y el orden son idénticos.
   - Lo apliqué porque el valor anterior no era reproducible: cambiaba en cada build.
   - Si se requiere conservar el formato anterior, se revierte su commit sin afectar lo demás.
3. **Pruebas que no enlazan (H13).** Corregir la línea de enlace en `CMakeLists.txt` está fuera de este alcance.
   Para ejecutarlas, las enlacé a mano con el mismo comando de ninja más `nsc_driver.lib nsc_basic.lib`
   (`nsc_relink_test.py`).

---

## 10. Pruebas y evidencia

{{TESTS}}

---

## 11. Pendientes y no verificado

- **Instrumentar `other`:** poda, publicación del bundle y destrucción de modelos (sección 3.6).
- **Causa de H11** (`oop-hir` de `Math.Core`) **y de H12** (`void*[]`).
- **Medir con `-g`.** La identidad de line tables incluye el digest de todos los buffers fuente, así que con
  depuración cualquier edición invalida todo. NO VERIFICADO con mediciones.
- **Juegos o proyectos EXE/DLL.** Las mediciones son de bibliotecas estáticas. Los EXE pasan además por el
  enlace incremental (`.nslinc`), que no se auditó aquí.

---

## Apéndice A. Reproducción

Scripts en `A:\temp\nsc-incremental-bench\`:

| Script | Uso |
|---|---|
| `nsc_incremental_bench.py` | cadena completa o una biblioteca |
| `nsc_scenarios.py` | escenarios con incremental, referencia, revert y equivalencia |
| `nsc_ab.py` | A/B entre `tc-base` y `tc-new` en O0–O3 |
| `nsc_compare.py` | comparación miembro a miembro de dos `.lib` |
| `nsc_collect.py` | consolida los `scenarios.json` |
| `nsc_scen_table.py` | tabla por fases de un `scenarios.json` |
| `nsc_relink_test.py` | enlaza las pruebas con las bibliotecas faltantes (H13) |
| `nsc_toolchain.py` | arma `tc-*` con layout instalado |

Ejemplo:

```
py -3 nsc_ab.py ab1 O0,O1,O2,O3 NativeSharp.Logging,NativeSharp.Math.Functions
```
