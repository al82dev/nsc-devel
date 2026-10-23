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
2. **Con el compilador base, la caché LLVM sólo sobrevive a ediciones de cuerpo o de comentarios (HECHO).**
   Agregar un método
   privado, un método público o un campo privado invalida el 100 % de las unidades en los cuatro niveles. Hay
   dos causas:
   - la identidad MIR incluye un digest común con *todas* las tablas del modelo;
   - incluye ordinales globales de funciones y miembros, que se recorren al insertar una declaración.
3. **Un build incremental puede producir un `.lib` distinto del build limpio de las mismas fuentes (HECHO).**
   En `NativeSharp.Math.Functions`, la edición de un solo cuerpo dio objetos distintos en O0, O1, O2 y O3. La
   causa es el plan de partición `CacheAwareBalanced`: se restaura del historial de la caché, así que el reparto
   de funciones entre unidades depende de compilaciones anteriores. En O1–O3 esto cambia además el código
   optimizado, porque el inlining sólo ocurre dentro de una unidad.
   - **Esto contradecía la equivalencia estricta.**
   - **Corregido en esta rama (L3, commit `712b3137c`)**, con autorización del usuario del 2026-10-22 para
     cambiar la política y la verificación de prueba que fijaba el diseño anterior (sección 8.7):
     - la equivalencia se cumple en los 52 escenarios, en O0, O1, O2 y O3;
     - agregar un método reutiliza 7 de 9 unidades en O0 y 14 de 16 en O1–O3 (antes 0);
     - el build en frío no se volvió más lento (fase LLVM, sección 8.7).
4. **El `.lib` cambiaba byte a byte entre dos builds de la misma entrada (HECHO).** Los nombres de los miembros
   del archivo COFF llevaban la ruta temporal `…\Temp\nativesharp-codegen-<reloj>-0\program-N.obj`.
   - **Corregido en esta rama.** Los miembros ahora son `program-N.obj`.
5. **Un build sin cambios (acierto de bundle) pagaba la materialización completa de NSMETA de todas las
   referencias antes de consultar el bundle (HECHO).**
   - **Corregido en esta rama:** la consulta ocurre antes de materializar y un acierto verificado la omite.
   - **Medido:** ahorra ~85 ms (−14 %) de `precompiled-metadata` en `Math.Functions` (sección 8.5). La parte
     mayor del acierto sigue siendo decodificar el NSMETA de las referencias. El paso siguiente (L0b) está
     diseñado.
6. **Reutilización de objetos por IR previo a la optimización (implementada en esta rama).**
   - **Qué hace:** cuando falla la identidad MIR, se baja la unidad a LLVM IR (barato) y se busca un objeto
     compilado desde IR idéntico. Sólo se optimiza y emite si no existe.
   - **Por qué es sólido:** optimizar y emitir es una función determinista del IR, del target y del nivel, así
     que conserva la equivalencia estricta.
   - **Dónde ayuda:** es justo el costo que domina en O2/O3 (10.5 s de 18.4 s en `Math.Functions`).
   - **Medición (sección 8.4).** Al insertar un campo estático privado en `Math.Functions`:
     - con el compilador base, 0 de 16 unidades se reutilizan;
     - con la rama, 15 de 16;
     - la fase LLVM baja de 10.6 s a 1.3 s en O2 (de 10.9 a 1.3 s en O3, de 7.2 a 1.4 s en O1, de 2.9 a
       1.4 s en O0);
     - el total baja de ~18.4 a ~9.0 s en O2.
   - **Salida.** Idéntica al build limpio en todos los escenarios, salvo `s2-body` de `Math.Functions`, que
     también difiere con el compilador base: es H3, no un efecto de la rama.
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

### 4.1 Build en frío por nivel (cadena completa, `nsc` instalado, ms)

Fases agrupadas: `meta` = `precompiled-metadata`; `syntax` = read+comptime+precompiled+lex+parse; `decl` = names+members+inheritance+interfaces+generic-constraints; `oopsem` = virtual-dispatch+destruction+construction+layout; `nsmeta` = escritura `library-metadata`; `other` = total − fases (sección 3.6).


**O0** — total de la cadena (33 paquetes: 32 compilan y `NativeForms` falla en ~0.2 s, H12): 78.1 s

| Biblioteca | total | graph | meta | syntax | generics | decl | bodies | oopsem | hir | mir | llvm | lib | hdr | nsmeta | other | unidades |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Core | 395 | 26 | 5 | 14 | 30 | 9 | 24 | 1 | 4 | 7 | 75 | 45 | 4 | 32 | 118 | 8 |
| System | 574 | 41 | 91 | 19 | 64 | 35 | 17 | 2 | 11 | 14 | 97 | 46 | 4 | 26 | 108 | 8 |
| Logging | 713 | 40 | 121 | 22 | 77 | 55 | 16 | 2 | 14 | 14 | 123 | 49 | 5 | 27 | 149 | 8 |
| Math.Matrices | 5798 | 83 | 388 | 65 | 1277 | 296 | 219 | 395 | 199 | 119 | 1201 | 133 | 5 | 255 | 1163 | 8 |
| Math.Functions | 16271 | 107 | 645 | 140 | 3968 | 653 | 974 | 661 | 622 | 207 | 2958 | 324 | 5 | 1284 | 3724 | 8 |
| Math.Core | 5899 | 37 | 1993 | 121 | 1176 | 291 | 8 | 9 | 1059 | 77 | 309 | 66 | 4 | 10 | 738 | 8 |

**O1** — total de la cadena (33 paquetes: 32 compilan y `NativeForms` falla en ~0.2 s, H12): 79.5 s

| Biblioteca | total | graph | meta | syntax | generics | decl | bodies | oopsem | hir | mir | llvm | lib | hdr | nsmeta | other | unidades |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Core | 510 | 27 | 5 | 14 | 67 | 9 | 11 | 1 | 4 | 7 | 169 | 47 | 4 | 31 | 115 | 15 |
| System | 700 | 41 | 101 | 20 | 65 | 34 | 16 | 2 | 11 | 13 | 211 | 45 | 4 | 25 | 115 | 15 |
| Logging | 870 | 37 | 119 | 22 | 76 | 54 | 16 | 2 | 15 | 14 | 319 | 49 | 5 | 27 | 114 | 15 |
| Math.Matrices | 5917 | 85 | 382 | 64 | 1095 | 294 | 224 | 404 | 195 | 128 | 1858 | 131 | 5 | 265 | 786 | 15 |
| Math.Functions | 19162 | 104 | 814 | 142 | 4156 | 657 | 953 | 617 | 620 | 206 | 7148 | 226 | 5 | 1331 | 2183 | 15 |
| Math.Core | 3820 | 36 | 1021 | 116 | 607 | 296 | 7 | 10 | 1056 | 70 | 241 | 40 | 4 | 11 | 305 | 15 |

**O2** — total de la cadena (33 paquetes: 32 compilan y `NativeForms` falla en ~0.2 s, H12): 90.1 s

| Biblioteca | total | graph | meta | syntax | generics | decl | bodies | oopsem | hir | mir | llvm | lib | hdr | nsmeta | other | unidades |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Core | 509 | 27 | 5 | 14 | 30 | 9 | 11 | 1 | 4 | 7 | 225 | 47 | 4 | 32 | 93 | 15 |
| System | 651 | 42 | 62 | 20 | 64 | 33 | 16 | 2 | 11 | 13 | 231 | 46 | 5 | 27 | 80 | 15 |
| Logging | 748 | 38 | 92 | 26 | 83 | 54 | 16 | 2 | 14 | 14 | 241 | 48 | 4 | 26 | 89 | 15 |
| Math.Matrices | 5377 | 82 | 278 | 63 | 522 | 303 | 214 | 392 | 182 | 111 | 2416 | 88 | 6 | 261 | 458 | 15 |
| Math.Functions | 18421 | 105 | 476 | 144 | 1279 | 653 | 954 | 625 | 624 | 254 | 10516 | 223 | 5 | 1270 | 1294 | 15 |
| Math.Core | 6216 | 34 | 1779 | 117 | 1823 | 291 | 8 | 10 | 1062 | 71 | 247 | 46 | 4 | 11 | 713 | 15 |

**O3** — total de la cadena (33 paquetes: 32 compilan y `NativeForms` falla en ~0.2 s, H12): 86.2 s

| Biblioteca | total | graph | meta | syntax | generics | decl | bodies | oopsem | hir | mir | llvm | lib | hdr | nsmeta | other | unidades |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Core | 509 | 28 | 5 | 15 | 31 | 9 | 11 | 1 | 4 | 7 | 207 | 43 | 4 | 31 | 113 | 15 |
| System | 874 | 42 | 91 | 19 | 64 | 33 | 16 | 2 | 11 | 12 | 217 | 44 | 4 | 27 | 291 | 15 |
| Logging | 916 | 39 | 230 | 22 | 77 | 53 | 16 | 2 | 14 | 14 | 249 | 45 | 4 | 27 | 124 | 15 |
| Math.Matrices | 7210 | 170 | 656 | 64 | 1301 | 295 | 215 | 421 | 187 | 114 | 2487 | 126 | 4 | 258 | 912 | 15 |
| Math.Functions | 19649 | 101 | 881 | 143 | 2038 | 649 | 998 | 620 | 618 | 206 | 10577 | 203 | 5 | 1329 | 1282 | 15 |
| Math.Core | 3873 | 34 | 1031 | 121 | 600 | 306 | 8 | 10 | 1073 | 80 | 251 | 40 | 5 | 11 | 304 | 15 |

### 4.2 Escenarios con el compilador base (`tc-base` = `991eead84`, host O3)

Tiempos en ms (`timing total_ms`). Aciertos = objetos restaurados de la caché de artefactos / unidades de código + `globals`. `equivalente` = salida incremental idéntica a la del build limpio (`.nsmeta`, `.h` y digest del `.lib` sin la tabla `//`).



| Nivel | Biblioteca | Escenario | incremental ms | referencia ms | aciertos LLVM / unidades+1 | bundle | equivalente |
|---|---|---|---:|---:|---:|---|---|
| O0 | Logging | s0-prime | 12512 | — | 0/9 | not-found |  |
| O0 | Logging | s1-nochange | 1548 | 1015 | (bundle) | hit | sí |
| O0 | Logging | s1b-touch | 126 | 652 | (bundle) | hit | sí |
| O0 | Logging | s5-comment | 571 | 656 | 9/9 | not-found | sí |
| O0 | Logging | s2-body | 577 | 647 | 8/9 | not-found | sí |
| O0 | Logging | s3-private-method | 618 | 687 | 0/9 | not-found | sí |
| O0 | Logging | s4-public-method | 652 | 712 | 0/9 | not-found | sí |
| O0 | Logging | s6-private-field | 613 | 651 | 0/9 | not-found | sí |
| O0 | Math.Functions | s0-prime | 11333 | — | 0/9 | not-found |  |
| O0 | Math.Functions | s1-nochange | 991 | 16008 | (bundle) | hit | sí |
| O0 | Math.Functions | s1b-touch | 3042 | 16373 | (bundle) | hit | sí |
| O0 | Math.Functions | s5-comment | 9293 | 11817 | 9/9 | not-found | sí |
| O0 | Math.Functions | s2-body | 10659 | 13002 | 8/9 | not-found | **NO** |
| O0 | Math.Functions | s3-private-method | 16512 | 15906 | 0/9 | not-found | sí |
| O0 | Math.Functions | s4-public-method | 11013 | 11476 | 0/9 | not-found | sí |
| O1 | Logging | s0-prime | 869 | — | 0/16 | not-found |  |
| O1 | Logging | s1-nochange | 172 | 748 | (bundle) | hit | sí |
| O1 | Logging | s1b-touch | 125 | 758 | (bundle) | hit | sí |
| O1 | Logging | s5-comment | 568 | 754 | 16/16 | not-found | sí |
| O1 | Logging | s2-body | 581 | 746 | 15/16 | not-found | sí |
| O1 | Logging | s3-private-method | 728 | 740 | 0/16 | not-found | sí |
| O1 | Logging | s4-public-method | 686 | 743 | 0/16 | not-found | sí |
| O1 | Logging | s6-private-field | 666 | 789 | 0/16 | not-found | sí |
| O1 | Math.Functions | s0-prime | 15758 | — | 0/16 | not-found |  |
| O1 | Math.Functions | s1-nochange | 904 | 14876 | (bundle) | hit | sí |
| O1 | Math.Functions | s1b-touch | 2696 | 19524 | (bundle) | hit | sí |
| O1 | Math.Functions | s5-comment | 12790 | 19500 | 16/16 | not-found | sí |
| O1 | Math.Functions | s2-body | 10474 | 15158 | 15/16 | not-found | **NO** |
| O1 | Math.Functions | s3-private-method | 15215 | 20572 | 0/16 | not-found | sí |
| O1 | Math.Functions | s4-public-method | 19561 | 19391 | 0/16 | not-found | sí |
| O2 | Logging | s0-prime | 907 | — | 0/16 | not-found |  |
| O2 | Logging | s1-nochange | 167 | 769 | (bundle) | hit | sí |
| O2 | Logging | s1b-touch | 121 | 765 | (bundle) | hit | sí |
| O2 | Logging | s5-comment | 600 | 779 | 16/16 | not-found | sí |
| O2 | Logging | s2-body | 565 | 773 | 15/16 | not-found | sí |
| O2 | Logging | s3-private-method | 771 | 789 | 0/16 | not-found | sí |
| O2 | Logging | s4-public-method | 721 | 818 | 0/16 | not-found | sí |
| O2 | Logging | s6-private-field | 724 | 757 | 0/16 | not-found | sí |
| O2 | Math.Functions | s0-prime | 19198 | — | 0/16 | not-found |  |
| O2 | Math.Functions | s1-nochange | 902 | 22859 | (bundle) | hit | sí |
| O2 | Math.Functions | s1b-touch | 2645 | 23215 | (bundle) | hit | sí |
| O2 | Math.Functions | s5-comment | 12377 | 18529 | 16/16 | not-found | sí |
| O2 | Math.Functions | s2-body | 11462 | 20891 | 15/16 | not-found | **NO** |
| O2 | Math.Functions | s3-private-method | 26808 | 29414 | 0/16 | not-found | sí |
| O2 | Math.Functions | s4-public-method | 19506 | 21162 | 0/16 | not-found | sí |
| O3 | Logging | s0-prime | 902 | — | 0/16 | not-found |  |
| O3 | Logging | s1-nochange | 170 | 839 | (bundle) | hit | sí |
| O3 | Logging | s1b-touch | 170 | 773 | (bundle) | hit | sí |
| O3 | Logging | s5-comment | 536 | 785 | 16/16 | not-found | sí |
| O3 | Logging | s2-body | 598 | 918 | 15/16 | not-found | sí |
| O3 | Logging | s3-private-method | 741 | 776 | 0/16 | not-found | sí |
| O3 | Logging | s4-public-method | 757 | 895 | 0/16 | not-found | sí |
| O3 | Logging | s6-private-field | 716 | 782 | 0/16 | not-found | sí |
| O3 | Math.Functions | s0-prime | 20915 | — | 0/16 | not-found |  |
| O3 | Math.Functions | s1-nochange | 923 | 22459 | (bundle) | hit | sí |
| O3 | Math.Functions | s1b-touch | 2579 | 26090 | (bundle) | hit | sí |
| O3 | Math.Functions | s5-comment | 9208 | 19197 | 16/16 | not-found | sí |
| O3 | Math.Functions | s2-body | 16233 | 27591 | 15/16 | not-found | **NO** |
| O3 | Math.Functions | s3-private-method | 25050 | 21043 | 0/16 | not-found | sí |
| O3 | Math.Functions | s4-public-method | 20524 | 19708 | 0/16 | not-found | sí |

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
| H1 | HECHO, corregido (ganancia menor: −14 % de `precompiled-metadata`) | El acierto de bundle materializaba todos los módulos precompilados antes de consultar el bundle; lo dominante es decodificar el NSMETA | `configurePrecompiledModules`: `preparePrecompiledProject` antes de `calculateIdentity`; s1 en `Math.Functions`: 615 → 532 ms (8 rondas, sección 8.5) |
| H2 | HECHO, corregido | El `.lib` difiere byte a byte entre builds idénticos: nombres de miembro con ruta temporal y reloj | `nsc_compare.py`: payloads idénticos, tabla `//` distinta (`nativesharp-codegen-2358…` contra `2361…`) |
| H3 | HECHO, **corregido (L3, `712b3137c`, autorizado)** | La salida incremental depende del historial de la caché: el plan `CacheAwareBalanced` restaurado no coincide con el reparto de un build limpio | s2-body en `Math.Functions`: `equal False` en O0, O1, O2 y O3; objetos `program-1..8` con tamaños distintos y `program-0` (globals) idéntico |
| H4 | HECHO | El número de unidades depende de los hilos de la máquina en O1–O3 (`partitionCount = workerCount`). El `.lib`, y en O1–O3 el código optimizado (inlining sólo dentro de una unidad), dependen de la máquina | `ProjectOopObjectGenerator::generate`; `code-units: 15` aquí |
| H5 | HECHO | La identidad MIR usa ordinales globales; cualquier inserción invalida casi todo | sección 5.1 |
| H6 | HECHO | El digest común cubre todo el modelo; un campo en una clase invalida todas las unidades | s6-private-field: 0/9 y 0/16 |
| H7 | HECHO, **corregido (L3)** | La clave del plan incluye el conjunto de funciones; agregar una función redistribuye todo | `persistentPlanKey` |
| H8 | HECHO | En un acierto de bundle, el reporte `--time` reimprime las líneas `code-unit` del build original (`miss-reason=cold`) | log de s1-nochange |
| H9 | HECHO | Cada acierto LLVM paga copia y verificación COFF (≈14 ms por unidad), y el `.lib` se vuelve a armar si cambió cualquier objeto | líneas `code-unit` de s5-comment |
| H10 | HECHO | Sin reutilización del frontend; pico de 4.6–5.2 GB en `Math.Functions` (`method-bodies` sube de 0.7 a 2.6 GB) | `peak_working_set_bytes` por fase |
| H11 | HECHO, causa NO VERIFICADA | `Math.Core`: ~1 s de `oop-hir` con 8 ms de `method-bodies` | tabla en frío, sección 4 |
| H12 | HECHO, causa NO VERIFICADA | Regresión: `new void*[7]` y `stackalloc void*[n]` rechazados con `void cannot be used as an allocation element type` (NativeForms y selfhost) | logs de la cadena; también con `tc-base` |
| H13 | HECHO, **corregido (`89a0550bb`)** | Las pruebas `nsc_project_oop_lowering_tests`, `nsc_namespace_function_llvm_tests` y `nsc_compilation_artifact_cache_tests` **no enlazan** con el `CMakeLists.txt` actual: faltan `nsc_basic` y `nsc_driver` en su línea de enlace (`kCompilerBuildId`, `CompilationGraphScheduler::clear`, `writeFileTransactionally`) | mismo error en `991eead84` y en esta rama |
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
| L0b | Acierto de bundle sin decodificar NSMETA | Lista de entradas resueltas + SHA-256 guardada en el bundle, validada rehasheando | Diseñado (8.5); requiere auditar la resolución de referencias |
| L1 | Archivo `.lib` determinista | Nombres de miembro estables | **Implementado** (H2) |
| L2 | Objeto por unidad desde IR idéntico | SHA-256 del IR previo a la optimización + target + nivel + pipeline + identidad de codegen + inlining obligatorio | **Implementado** |
| L3 | Partición determinista y estable | Función pura del MIR: cubetas por hash del nombre decorado, con Balanced cuando alguna queda vacía | **Implementado** (`712b3137c`, sección 8.7) |
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
- Con `CacheAwareBalanced`, agregar una función redistribuía todas las unidades (H7). Su IR cambiaba y L2 no
  ayudaba en ese escenario. Con L3 (sección 8.7) sólo cambia la unidad que recibe la función nueva.

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

### 8.1 Commits de la rama

| Commit | Hipótesis | Archivos |
|---|---|---|
| `a7b592c90` | H2: nombres estables de los miembros del `.lib` (`program-N.obj`) | `src/Linker/LldLinker.cpp` + este documento |
| `07b53c6d7` | L2: reutilizar objetos por IR previo a la optimización cuando falla la identidad MIR | `ProjectOopObjectGenerator.cpp`, `LlvmOptimizer.h`, `CodeUnitTelemetry.h`, `CodeUnitIrReuse.h` (nuevo), `DriverExecution.cpp` |
| `275874387` | H1: consultar el bundle antes de materializar los módulos precompilados | `PrecompiledProjectPreparation.{h,cpp}`, `PrecompiledModuleConfiguration.cpp` |
| `e88f0b44d` | Documento con resultados y corrección del comentario de motivo en `PrecompiledProjectPreparation.cpp` | este documento + comentario |
| `712b3137c` | L3: partición `DeterministicIncremental` en todo modo de caché y LTO (autorizado); pruebas ajustadas y nueva verificación de equivalencia | `ProjectOopCodePartitionPolicy.h`, `ProjectOopCodePartition.cpp`, `ProjectOopObjectGenerator.cpp`, `tests/unit/ProjectOopLoweringTests.cpp` |
| `89a0550bb` | H13: `nsc_compilation` enlaza `nsc_basic` y contiene `CompilationGraphScheduler.cpp`; todas las pruebas enlazan | `CMakeLists.txt` |

- **Cómo se compiló.** Los tres cambios se compilaron y probaron juntos, en el estado final de la rama.
- **Qué no se verificó.** No se compiló cada commit intermedio por separado (NO VERIFICADO).
  - Los archivos de cada uno son disjuntos.
  - Ninguno depende de los otros.

### 8.2 Equivalencia (HECHO)

- **Incremental contra limpio, por compilador.** Con `tc-base` y con `tc-new` la salida incremental es idéntica
  a la del build limpio en **todos** los escenarios y niveles, salvo `s2-body` en `Math.Functions` (O0–O3).
  - Esa excepción la tienen **los dos** compiladores: es H3 (plan de partición restaurado), no un efecto de
    esta rama.
  - En `tc-new` la comparación incluye el `.lib` byte a byte, gracias a H2.
- **Base contra nueva (build limpio).** `nsc_crosscheck.py` / `nsc_idcheck.py` sobre los 52 pares
  (2 bibliotecas × 4 niveles × escenarios).
  - Los **objetos** de cada `.lib` son idénticos y en el mismo orden en los 52.
  - El `.h` es idéntico tras sustituir la identidad de build del compilador
    (`3.0.0+build.<fecha>.<id>`), que aparece en el comentario `Generated by nsc …`.
  - El `.nsmeta` difiere en 49 bytes: la identidad de build y el digest de 32 bytes que la cubre. Ninguna
    otra diferencia.
  - Que dos builds de `nsc` distintos escriban identidades distintas es el comportamiento esperado.

### 8.3 A/B por escenario (O0–O3)

Las cachés de las dos corridas son independientes (`ab1-base`, `ab1-new`). Los tiempos de una sola corrida
tienen ruido alto (sección 2.3). Los aciertos son exactos.

| Nivel | Biblioteca | Escenario | base ms | nueva ms | aciertos base | aciertos nueva | equivalente base | equivalente nueva |
|---|---|---|---:|---:|---:|---:|---|---|
| O0 | Logging | s0-prime | 12512 | 856 | 0/9 | 0/9 | — | — |
| O0 | Logging | s1-nochange | 1548 | 193 | bundle | bundle | sí | sí |
| O0 | Logging | s1b-touch | 126 | 135 | bundle | bundle | sí | sí |
| O0 | Logging | s5-comment | 571 | 817 | 9/9 | 9/9 | sí | sí |
| O0 | Logging | s2-body | 577 | 725 | 8/9 | 8/9 | sí | sí |
| O0 | Logging | s3-private-method | 618 | 678 | 0/9 | 0/9 | sí | sí |
| O0 | Logging | s4-public-method | 652 | 917 | 0/9 | 0/9 | sí | sí |
| O0 | Logging | s6-private-field | 613 | 1012 | 0/9 | **9/9** | sí | sí |
| O0 | Math.Functions | s0-prime | 11333 | 16415 | 0/9 | 0/9 | — | — |
| O0 | Math.Functions | s1-nochange | 991 | 3719 | bundle | bundle | sí | sí |
| O0 | Math.Functions | s1b-touch | 3042 | 819 | bundle | bundle | sí | sí |
| O0 | Math.Functions | s5-comment | 9293 | 9231 | 9/9 | 9/9 | sí | sí |
| O0 | Math.Functions | s2-body | 10659 | 14698 | 8/9 | 8/9 | **NO** | **NO** |
| O0 | Math.Functions | s3-private-method | 16512 | 11462 | 0/9 | 0/9 | sí | sí |
| O0 | Math.Functions | s4-public-method | 11013 | 16677 | 0/9 | 0/9 | sí | sí |
| O1 | Logging | s0-prime | 869 | 887 | 0/16 | 0/16 | — | — |
| O1 | Logging | s1-nochange | 172 | 154 | bundle | bundle | sí | sí |
| O1 | Logging | s1b-touch | 125 | 111 | bundle | bundle | sí | sí |
| O1 | Logging | s5-comment | 568 | 571 | 16/16 | 16/16 | sí | sí |
| O1 | Logging | s2-body | 581 | 584 | 15/16 | 15/16 | sí | sí |
| O1 | Logging | s3-private-method | 728 | 738 | 0/16 | 0/16 | sí | sí |
| O1 | Logging | s4-public-method | 686 | 733 | 0/16 | 0/16 | sí | sí |
| O1 | Logging | s6-private-field | 666 | 565 | 0/16 | **16/16** | sí | sí |
| O1 | Math.Functions | s0-prime | 15758 | 15734 | 0/16 | 0/16 | — | — |
| O1 | Math.Functions | s1-nochange | 904 | 831 | bundle | bundle | sí | sí |
| O1 | Math.Functions | s1b-touch | 2696 | 2841 | bundle | bundle | sí | sí |
| O1 | Math.Functions | s5-comment | 12790 | 12116 | 16/16 | 16/16 | sí | sí |
| O1 | Math.Functions | s2-body | 10474 | 10510 | 15/16 | 15/16 | **NO** | **NO** |
| O1 | Math.Functions | s3-private-method | 15215 | 17330 | 0/16 | 0/16 | sí | sí |
| O1 | Math.Functions | s4-public-method | 19561 | 19635 | 0/16 | 0/16 | sí | sí |
| O2 | Logging | s0-prime | 907 | 1507 | 0/16 | 0/16 | — | — |
| O2 | Logging | s1-nochange | 167 | 188 | bundle | bundle | sí | sí |
| O2 | Logging | s1b-touch | 121 | 182 | bundle | bundle | sí | sí |
| O2 | Logging | s5-comment | 600 | 683 | 16/16 | 16/16 | sí | sí |
| O2 | Logging | s2-body | 565 | 589 | 15/16 | 15/16 | sí | sí |
| O2 | Logging | s3-private-method | 771 | 934 | 0/16 | 0/16 | sí | sí |
| O2 | Logging | s4-public-method | 721 | 789 | 0/16 | 0/16 | sí | sí |
| O2 | Logging | s6-private-field | 724 | 641 | 0/16 | **16/16** | sí | sí |
| O2 | Math.Functions | s0-prime | 19198 | 22560 | 0/16 | 0/16 | — | — |
| O2 | Math.Functions | s1-nochange | 902 | 1455 | bundle | bundle | sí | sí |
| O2 | Math.Functions | s1b-touch | 2645 | 862 | bundle | bundle | sí | sí |
| O2 | Math.Functions | s5-comment | 12377 | 24613 | 16/16 | 16/16 | sí | sí |
| O2 | Math.Functions | s2-body | 11462 | 14099 | 15/16 | 15/16 | **NO** | **NO** |
| O2 | Math.Functions | s3-private-method | 26808 | 23771 | 0/16 | 0/16 | sí | sí |
| O2 | Math.Functions | s4-public-method | 19506 | 18500 | 0/16 | 0/16 | sí | sí |
| O3 | Logging | s0-prime | 902 | 1149 | 0/16 | 0/16 | — | — |
| O3 | Logging | s1-nochange | 170 | 196 | bundle | bundle | sí | sí |
| O3 | Logging | s1b-touch | 170 | 217 | bundle | bundle | sí | sí |
| O3 | Logging | s5-comment | 536 | 1192 | 16/16 | 16/16 | sí | sí |
| O3 | Logging | s2-body | 598 | 1428 | 15/16 | 15/16 | sí | sí |
| O3 | Logging | s3-private-method | 741 | 1125 | 0/16 | 0/16 | sí | sí |
| O3 | Logging | s4-public-method | 757 | 931 | 0/16 | 0/16 | sí | sí |
| O3 | Logging | s6-private-field | 716 | 670 | 0/16 | **16/16** | sí | sí |
| O3 | Math.Functions | s0-prime | 20915 | 25666 | 0/16 | 0/16 | — | — |
| O3 | Math.Functions | s1-nochange | 923 | 832 | bundle | bundle | sí | sí |
| O3 | Math.Functions | s1b-touch | 2579 | 2851 | bundle | bundle | sí | sí |
| O3 | Math.Functions | s5-comment | 9208 | 13100 | 16/16 | 16/16 | sí | sí |
| O3 | Math.Functions | s2-body | 16233 | 11736 | 15/16 | 15/16 | **NO** | **NO** |
| O3 | Math.Functions | s3-private-method | 25050 | 19516 | 0/16 | 0/16 | sí | sí |
| O3 | Math.Functions | s4-public-method | 20524 | 23797 | 0/16 | 0/16 | sí | sí |

**Lectura:**

- **`s6-private-field`.** Pasa de 0 aciertos a **9/9 (O0) y 16/16 (O1–O3)**. La identidad MIR falla en todas
  las unidades, pero su IR no cambió, así que no se optimiza ni emite nada.
- **`s3`/`s4`.** Siguen en 0 en ambos. Agregar un método cambia `persistentPlanKey`, `CacheAwareBalanced`
  redistribuye todas las funciones y cambia el IR de cada unidad (H7). Ver 8.6.
- **Comentario y cuerpo.** No cambian; ya acertaban por MIR.

### 8.4 Repeticiones controladas: L2

`nsc_irbench_mf.py` alterna los dos compiladores en cada ronda. Cada uno usa su caché caliente. Inserta un
`private static float irBenchStaticN = (N+1).0f;` distinto en cada ronda en `Math.Functions`. La identidad MIR
cambia (nombre y constante en el digest común); el reparto no cambia (no se agregan funciones).

| Nivel | Ronda | base total / llvm (ms), aciertos | nueva total / llvm (ms), aciertos |
|---|---|---|---|
| O0 | 1 | 10932 / 2808, 0/9 | 9490 / 1337, 8/9 |
| O0 | 2 | 11073 / 2959, 0/9 | 11666 / 1416, 8/9 |
| O1 | 1 | 15074 / 7242, 0/16 | 9205 / 1368, 15/16 |
| O1 | 2 | 16744 / 7100, 0/16 | 13005 / 1437, 15/16 |
| O2 | 1 | 18490 / 10678, 0/16 | 9005 / 1344, 15/16 |
| O2 | 2 | 18378 / 10632, 0/16 | 8879 / 1292, 15/16 |
| O3 | 1 | 24058 / 10950, 0/16 | 12901 / 1331, 15/16 |
| O3 | 2 | 23003 / 10662, 0/16 | 9030 / 1289, 15/16 |

- **Ronda 0** (primera inserción). 0 aciertos en ambos: el campo estático nuevo agrega una función de
  inicialización, eso cambia el plan y se repite H7.
- **Fase `llvm-object-generation` con L2.** Baja de ~2.9/7.2/10.6/10.8 s a ~1.3–1.4 s en O0/O1/O2/O3.
- **Lo que queda en la fase LLVM** (~1.3 s), según las líneas `code-unit` de O2:
  - la unidad `globals`, que sí cambia;
  - por cada unidad que acierta, el bajado a IR (~75 ms), la impresión y el hash (~70 ms), la copia (~40 ms)
    y la verificación COFF (~8–19 ms);
  - el preámbulo (alcanzabilidad e índice de identidad MIR), sin desglose (NO VERIFICADO).
- **El total** baja ~9–10 s en O2/O3. El resto es el frontend completo (sección 7.4).

`nsc_irbench.py`, mismo método en `Logging` con 2–7 campos de instancia nuevos por ronda (6 rondas por nivel,
mediana de la fase LLVM):

| Nivel | base | nueva |
|---|---:|---:|
| O0 | 160 ms (0 aciertos) | 126 ms (4–9/9) |
| O2 | 255 ms (0 aciertos) | 160 ms (11–16/16) |
| O3 | 253 ms (0 aciertos) | 160 ms (11–16/16) |

En las rondas que llegan a 16/16, hasta las unidades de `Logger` aciertan: el número de campos `int`
insertados dejó los mismos desplazamientos tras el relleno de alineación. Esto confirma que la clave de IR
reutiliza exactamente cuando el código generado sería el mismo.

### 8.5 Repeticiones controladas: H1 (acierto de bundle)

`nsc_hitbench.py`, 8 rondas alternadas, `Math.Functions` O0 sin cambios:

| | `precompiled-metadata` (mediana) | total mínimo | total mediana |
|---|---:|---:|---:|
| base | 615 ms | 892 ms | 1708 ms |
| nueva | 532 ms | 808 ms | 1394 ms |

- **H1 ahorra ~85 ms (−14 %)** de `precompiled-metadata`. Es estable en las 8 rondas: 608–623 ms contra
  529–540 ms.
- **La parte mayor del acierto sigue siendo cargar y decodificar el NSMETA** de las referencias para calcular la
  identidad. Esta medición corrige la expectativa inicial: materializar no dominaba.
- **El siguiente paso es L0b (diseño):** guardar en la entrada del bundle la lista completa de entradas
  resueltas con su SHA-256, y validar el acierto rehasheando esos archivos sin decodificarlos. Antes hay que
  auditar `resolveBinaryReference`: una referencia resuelta por búsqueda en varias raíces puede cambiar si
  aparece un archivo nuevo en una raíz anterior.
- **La varianza de 0.8–3.4 s del total** está fuera de las fases medidas: restauración de ~25 MB de salidas y
  probablemente escaneo de antivirus (NO VERIFICADO).

### 8.6 Prototipo de partición determinista (evidencia previa a la decisión 9.1; reemplazado por 8.7)

- **Qué es.** Un worktree aparte (`A:\nsc-devel-proto-partition`, desde `275874387`) con el parche
  `proto-partition.patch`. El generador usa siempre una política `DeterministicIncremental`: cubetas por hash
  del nombre decorado y Balanced si alguna queda vacía. No consulta ni guarda planes.
- **Cómo se compiló.** Igual que las demás: clang-cl, Release, host O3.
- **No está en la rama.**
- **Escenarios** (`tc-proto`, cachés `p1`):

| Nivel | Biblioteca | s2-body equivalente | s3-private-method aciertos | s4-public-method aciertos | s6-private-field aciertos |
|---|---|---|---|---|---|
| O0 | Math.Functions | **sí** (base y nueva: **NO**) | 7/9 (antes 0/9) | 7/9 (antes 0/9) | — |
| O0 | Logging | sí | 7/9 (antes 0/9) | 7/9 (antes 0/9) | 9/9 |
| O2 | Math.Functions | **sí** (base y nueva: **NO**) | 14/16 (antes 0/16) | 14/16 (antes 0/16) | — |
| O2 | Logging | sí | 14/16 (antes 0/16) | 14/16 (antes 0/16) | 16/16 |

- **En todos los escenarios de las dos bibliotecas, en O0 y O2, la salida incremental coincidió con la del
  build limpio** (`equal True`), incluido el `.lib` byte a byte.
- **Al agregar un método** sólo se recompilan la unidad que recibe la función nueva y `globals` (2 de 9 o de 16).
- **Tiempo.** `Math.Functions` O2 con método privado: 14.4 s, contra 23–27 s en base y nueva. La reducción
  viene casi entera de LLVM. El resto es el frontend.
- **Lo que este prototipo NO resuelve:**
  1. **H4.** El número de unidades sigue dependiendo de los hilos en O1–O3.
  2. **Balance en frío.** Las cubetas por hash no balancean por costo. No se midió de forma controlada si un
     build en frío O2/O3 se vuelve más lento; en esta corrida, `Math.Functions` O2 en frío dio 27.0 s, con
     ruido alto.
  3. **La prueba.** Ver la tabla siguiente.
- **Pruebas sobre el prototipo** (`nsc_project_oop_lowering_tests`, enlazada como en la sección 10):

| Ejecutable | `991eead84` | esta rama (`275874387`) | prototipo L3 |
|---|---|---|---|
| `nsc_project_oop_lowering_tests` | 603/629; 26 fallas de ambiente/fixtures | 603/629; **las mismas 26** | 602/629: las mismas 26 **más** *"cold generation atomically publishes its private code-partition plan"* |

El prototipo rompe exactamente la verificación que fija el plan `CacheAwareBalanced` y ninguna otra. En
particular siguen pasando:

- *"one body edit invalidates exactly its code bucket while reusing coordinator and peers"*;
- *"second equivalent generation restores every verified code unit"*;
- la verificación del límite de 8 unidades en O0.




### 8.7 L3 aplicado: partición `DeterministicIncremental` (commit `712b3137c`)

- **Autorización.** El usuario autorizó el 2026-10-22 cambiar la política y la verificación de prueba que fijaba
  `CacheAwareBalanced`.
- **Qué cambió.**
  - `buildObjectPartition` usa `DeterministicIncremental` en todo modo de caché y de LTO.
  - Ya no calcula clave de plan ni restaura planes. Con la clave vacía, `finishCodeUnitBundle` no guarda
    ninguno.
  - `CacheAwareBalanced`, su payload y la API de planes de la caché quedan intactos y probados.
- **Diferencia con el prototipo (8.6).** El prototipo cambiaba lo mismo, pero sin pruebas ajustadas ni
  documentación.
- **Pruebas ajustadas** (`tests/unit/ProjectOopLoweringTests.cpp`):
  - *"cold generation publishes no code-partition plan: the partition is a pure function of the MIR"*
    reemplaza a la verificación que exigía publicar el plan;
  - la verificación de restauración de `CacheAwareBalanced` usa el payload de la partición en frío;
  - la verificación de poda guarda un plan a mano, para seguir cubriendo el desalojo de planes;
  - **nueva:** *"an incremental generation emits the same objects as a clean generation of the same sources"*,
    que compara byte a byte contra una generación con caché vacía.
- **Cambio de salida (esperado).** Los objetos particionados difieren de los del compilador anterior, porque el
  reparto ahora es por hash del nombre. El número de unidades no cambia: O0 min(hilos, 8); O1–O3, hilos (H4).

**Escenarios con `tc-l3`** (cachés `l3b`, una corrida; ms del build incremental / build limpio de referencia;
entre paréntesis, unidades reusadas de 9 en O0 y de 16 en O1–O3). En **los 52** la salida incremental es igual
a la del build limpio (`equal True`), incluido el `.lib` byte a byte.

| Nivel | Biblioteca | s5-comment | s2-body | s3-private-method | s4-public-method | s6-private-field |
|---|---|---|---|---|---|---|
| O0 | Math.Functions | 8831 / 10532 (9) | 15779 / 18061 (8) | 10198 / 11201 (7) | 10710 / 16706 (7) | — |
| O1 | Math.Functions | 10156 / 19966 (16) | 10849 / 16652 (15) | 14425 / 20365 (14) | 13145 / 15232 (14) | — |
| O2 | Math.Functions | 8928 / 18529 (16) | 12017 / 19539 (15) | 14738 / 23922 (14) | 14359 / 18245 (14) | — |
| O3 | Math.Functions | 9511 / 18826 (16) | 12128 / 20786 (15) | 14585 / 24018 (14) | 16241 / 18605 (14) | — |
| O0 | Logging | 582 / 972 (9) | 1100 / 1261 (8) | 743 / 778 (7) | 624 / 767 (7) | 744 / 697 (9) |
| O1 | Logging | 559 / 719 (16) | 577 / 729 (15) | 624 / 720 (14) | 614 / 743 (14) | 523 / 696 (16) |
| O2 | Logging | 560 / 773 (16) | 645 / 744 (15) | 613 / 798 (14) | 598 / 762 (14) | 560 / 810 (16) |
| O3 | Logging | 547 / 780 (16) | 642 / 800 (15) | 634 / 816 (14) | 611 / 756 (14) | 555 / 806 (16) |

- Antes de L3, `s3` y `s4` reusaban 0 unidades en todos los niveles (sección 4.2).
- Una sola corrida: los tiempos tienen ruido (sección 2.3); los conteos de unidades son exactos.
- Con un método nuevo, lo que queda es casi todo frontend (sección 7.4).

**Build en frío, `tc-new` (antes de L3) contra `tc-l3`** (`nsc_coldbench.py`, 3 rondas alternadas, caché
nueva en cada build; mediana en ms):

| Nivel | Math.Functions total | Math.Functions LLVM | Logging total | Logging LLVM |
|---|---|---|---|---|
| O0 | 15752 → 16579 | 3121 → 2670 | 726 → 659 | 157 → 127 |
| O1 | 17722 → 14713 | 7104 → 6838 | 1405 → 821 | 225 → 246 |
| O2 | 18902 → 18391 | 10726 → 10587 | 809 → 830 | 255 → 279 |
| O3 | 18657 → 20041 | 10581 → 10598 | 804 → 769 | 257 → 257 |

- **HECHO:** la fase LLVM, la única que cambia con la partición, no se volvió más lenta. En O0 y O1 bajó en
  `Math.Functions`.
- Los totales varían ±3 s entre rondas por el frontend; no son evidencia en ningún sentido.
- **INFERENCIA:** las cubetas por hash quedan bien balanceadas porque estas bibliotecas tienen cientos de
  funciones por unidad. En un programa con pocas funciones muy grandes el balance podría ser peor que LPT; no se
  midió (NO VERIFICADO).

**Cadena completa con `tc-l3`** (33 paquetes, caché nueva): en O0, O1, O2 y O3 compilan 32. `NativeForms` falla
igual que antes (H12). Tiempo total de la cadena: O0 72.9 s, O1 82.2 s, O2 88.9 s, O3 85.3 s; base con el `nsc`
instalado: 78.1, 79.5, 90.1 y 86.2 s (sección 4.1, otro momento: comparación sólo indicativa).

---

## 9. Decisiones (estado al 2026-10-22)

1. **Política de partición determinista (L3, corrige H3 y H7). RESUELTA: autorizada y aplicada
   (`712b3137c`, sección 8.7).** Lo que sigue es el planteamiento original.
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
   - **Evidencia del prototipo (8.6):**
     - corrige H3 (equivalencia en `s2-body`) y H7 (14/16 en lugar de 0/16 al agregar un método);
     - falla sólo *"cold generation atomically publishes its private code-partition plan"*.
   - **Además, desde H4:** para que el resultado no dependa de la máquina, el número de unidades en O1–O3 debe
     ser fijo o depender sólo del programa, no de los hilos. Eso cambia el código optimizado.
2. **Nombres de miembro del `.lib` (H2).**
   - Cambian bytes del artefacto emitido: la tabla de nombres largos.
   - El contenido de cada objeto y el orden son idénticos.
   - Lo apliqué porque el valor anterior no era reproducible: cambiaba en cada build.
   - Si se requiere conservar el formato anterior, se revierte su commit sin afectar lo demás.
   - **Evidencia** (sección 10): en la cadena completa O0, los objetos de las 32 bibliotecas son idénticos a los
     del `nsc` instalado.
3. **Pruebas que no enlazan (H13). RESUELTA (`89a0550bb`).** Con la autorización del usuario de hacer los
   cambios necesarios para mejorar el compilador, se corrigió la causa en `CMakeLists.txt`:
   - `nsc_compilation` usaba `nsc_basic` sin enlazarlo;
   - `CompilationGraphScheduler.cpp` vivía en `nsc_driver`, que depende de `nsc_frontend`, y `nsc_frontend` lo
     usa.

   Ahora `CompilationGraphScheduler.cpp` está en `nsc_compilation`, junto a `CompilationWorkerPool.cpp`, y
   `nsc_compilation` enlaza `nsc_basic`. En el build completo enlazan 431 ejecutables, con 0 símbolos
   indefinidos. Antes, las pruebas había que enlazarlas a mano con `nsc_relink_test.py`.

---

## 10. Pruebas y evidencia

**Cómo se ejecutaron (HECHO):**

- Las tres pruebas relacionadas con los archivos tocados no enlazan con el `CMakeLists.txt` actual, ni en
  `991eead84` ni en esta rama (H13).
- Se enlazaron con el mismo comando que genera ninja más `lib\nsc_driver.lib lib\nsc_basic.lib`
  (`nsc_relink_test.py`), sin tocar el repositorio.
- Se ejecutaron desde copias en `A:\temp\nsc-incremental-bench\tests-*`. Desde `A:\nsc-devel\build-incremental`
  el ambiente niega la creación de directorios (sección 2.1). Eso tumbó la primera corrida con `0xC0000409`;
  la causa es el ambiente, no el código.

| Ejecutable | `991eead84` (`tests-base`) | esta rama (`tests-new`) |
|---|---|---|
| `nsc_compilation_artifact_cache_tests` | PASS 31 | PASS 31 |
| `nsc_namespace_function_llvm_tests` | PASS 10 | PASS 10 |
| `nsc_project_oop_lowering_tests` | 603/629 | 603/629 |

- **Las 26 fallas de `nsc_project_oop_lowering_tests` son idénticas en texto y en orden en las dos
  revisiones.** Dos son el SDK de import libraries ausente en `C:/Program Files (x86)/NativeSharp/sdk`. Las otras
  24 son fixtures de agregados, enumeraciones y excepciones que no pasan por los archivos modificados.
- **Pasan en las dos revisiones** las verificaciones de caché y partición:
  - *"cold incremental build emits and stores every code unit"*;
  - *"a body-only edit preserves the private code-partition topology key"*;
  - *"cold generation atomically publishes its private code-partition plan"*;
  - *"one body edit invalidates exactly its code bucket while reusing coordinator and peers"*;
  - *"unchanged incremental rebuild restores every unit after the precise miss"*;
  - *"the artifact-cache byte budget evicts private partition plans with objects"*;
  - *"second equivalent generation restores every verified code unit from content cache"*;
  - la verificación del tope de unidades en O0 y la de "full-debug optimized generation remains monolithic".
- **Batería completa (después de L3 y H13, HECHO).** Se corrió la batería completa de CTest (686 pruebas) de
  `991eead84` (con sólo la corrección de enlace H13 aplicada en su worktree) y de esta rama (`89a0550bb`), con
  `nsc_suite.py`:
  - copia de cada build fuera de `A:\nsc-devel` (`A:\temp\nsc-suite\{base,l3}`), por el problema de ambiente
    de la sección 2.1;
  - mismo comando, directorio y ambiente que CTest, con la ruta del build sustituida;
  - `TEMP`/`TMP` dentro de la copia, para no escribir en `C:`;
  - reglas de CTest: `PASS_REGULAR_EXPRESSION`, `WILL_FAIL`, `RUN_SERIAL` y `DEPENDS`.

  | Revisión | pasan | fallan | no corren |
  |---|---|---|---|
  | `991eead84` + H13 | 387 | 297 | 2 |
  | esta rama (`89a0550bb`) | 387 | 297 | 2 |

  - **Las 686 pruebas tienen el mismo resultado y el mismo código de salida en las dos revisiones.**
  - En las 224 pruebas que imprimen *"FAILED: N of M checks"*, N es igual en las dos. La única diferencia de M es
    `nsc.project-oop-lowering`: 629 contra 630, por la verificación de equivalencia nueva de L3, que pasa.
  - **Las 297 fallas son de ambiente y no prueban nada en ninguno de los dos sentidos:**
    - 161 buscan el SDK instalado en `C:/Program Files (x86)/NativeSharp` (no existe, y no se escribe en `C:`);
    - otras buscan bibliotecas en `A:\nsc-devel\lib\…` o rutas del build original compiladas en la prueba;
    - el resto son ejecuciones de fixtures que fallan igual en la base (por ejemplo, `0xC00000FD`).
  - **Las 2 que no corren** son `nsc.ns3.t03.sum-types` y `nsc.ns3.t10.await-protocol`. Sus fuentes de prueba no
    compilan, **igual en `991eead84`** (`MatchArmCoverage` ganó `listLengths`; el inicializador de await quedó
    viejo). No se tocaron.
- **Batería anterior a L3/H13 (la que dice "no se ejecutó"):** reemplazada por el punto anterior.
- **Validación funcional propia:**
  - 52 pares base/nueva en build limpio, con objetos idénticos;
  - 120 builds incrementales (60 escenarios × 2 compiladores) comparados contra su build limpio;
  - la cadena completa de 33 bibliotecas con `tc-new` en O0, comparada con la del `nsc` instalado
    (`nsc_chaincheck.py`).
    - Las **32 bibliotecas que compilan** tienen objetos idénticos y el mismo orden.
    - El `.h` es idéntico tras normalizar la identidad de build.
    - El `.nsmeta` difiere sólo en el digest de identidad (31–32 bytes).
    - `NativeForms` falla igual en ambos (H12).
  - **La cadena completa con `tc-new` en O1–O3 no se corrió.** En esos niveles la equivalencia está cubierta por
    los escenarios de `Logging` y `Math.Functions`.
  - **Con `tc-l3` (después de L3) la cadena completa sí se corrió en O0, O1, O2 y O3:** compilan 32 de 33 en cada
    nivel, y `NativeForms` falla igual (H12, sección 8.7). Sus objetos difieren de los del `nsc` instalado por
    diseño (otro reparto), así que no se compararon byte a byte contra él.


---

## 11. Pendientes y no verificado

- **Instrumentar `other`:** poda, publicación del bundle y destrucción de modelos (sección 3.6).
- **Causa de H11** (`oop-hir` de `Math.Core`) **y de H12** (`void*[]`).
- **Medir con `-g`.** La identidad de line tables incluye el digest de todos los buffers fuente, así que con
  depuración cualquier edición invalida todo. NO VERIFICADO con mediciones.
- **Juegos o proyectos EXE/DLL.** Las mediciones son de bibliotecas estáticas. Los EXE pasan además por el
  enlace incremental (`.nslinc`), que no se auditó aquí.
- **H4.** El número de unidades en O1–O3 sigue dependiendo de los hilos de la máquina. No se cambió.
- **Balance de L3 en programas con pocas funciones muy grandes.** NO VERIFICADO (sección 8.7).
- **Las 297 pruebas que fallan por ambiente.** Para que prueben algo hace falta el SDK instalado en
  `C:/Program Files (x86)/NativeSharp` y las bibliotecas en `A:\nsc-devel\lib`. Eso no se hizo: no se escribe en
  `C:`.
- **Las mediciones con `nsc_incremental_bench.py` y `nsc_scenarios.py` no redirigen `TEMP`.** El `nsc` medido creó
  sus directorios temporales de codegen en el `TEMP` del proceso, `C:\Users\al82dev\AppData\Local\Temp`
  (HECHO). Ahí quedan 15 directorios `nativesharp-codegen-*`. No se sabe cuántos son de esta tarea y cuántos de
  builds anteriores (NO VERIFICADO), ni por qué no se borraron. No se tocaron. `nsc_suite.py` sí redirige `TEMP`
  a `A:`.

---

## Apéndice A. Reproducción

**Directorios creados en esta tarea.** Ninguno se ha borrado. Para borrarlos hace falta tu autorización.

- `A:\nsc-devel\build-incremental` (build de la rama).
- Worktree `A:\nsc-devel-baseline-991eead84` y su `build-baseline`.
- Worktree `A:\nsc-devel-proto-partition` y su `build-proto`.
- `A:\temp\nsc-incremental-bench` (banco, cachés y resultados).
- `A:\temp\nsc-suite\base` y `A:\temp\nsc-suite\l3`: copias de los builds para la batería completa
  (~15 GB cada una).
- **Modificado sin commit:** `A:\nsc-devel-baseline-991eead84\CMakeLists.txt` tiene la corrección H13, para
  poder enlazar y correr la batería de la base.



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
| `nsc_irbench.py`, `nsc_irbench_mf.py` | rondas alternadas de inserción de campos (L2) |
| `nsc_hitbench.py` | rondas alternadas sin cambios (H1) |
| `nsc_crosscheck.py`, `nsc_idcheck.py`, `nsc_chaincheck.py` | equivalencia entre compiladores |
| `msgs\proto-partition.patch` | parche del prototipo L3 (sección 8.6) |
| `nsc_coldbench.py` | builds en frío alternados `tc-new` / `tc-l3` (sección 8.7) |
| `nsc_suite.py` | copia de un build y batería de CTest fuera de `A:\nsc-devel` (sección 10) |
| `nsc_logrun.py` | corre un programa, guarda toda la salida en un log e imprime sólo las fallas |

Ejemplo:

```
py -3 nsc_ab.py ab1 O0,O1,O2,O3 NativeSharp.Logging,NativeSharp.Math.Functions
```
