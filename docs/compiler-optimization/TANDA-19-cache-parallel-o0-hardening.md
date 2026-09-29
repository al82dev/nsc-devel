# Tanda 19: caché confiable, cancelación acotada y cierre O0

## Correspondencia con el plan NativeSharp 3.0

Este commit cierra la **tanda operativa 03** del documento maestro: P05/P06/P07. Parte de las
implementaciones de las tandas históricas 07–11 (particiones estables, backend streaming, fast path
O0, caché antes de LLVM y materialización final), audita sus contratos reales y corrige los huecos
que todavía permitían reutilización incorrecta, publicación concurrente parcial, trabajo inútil
después de un fallo y rearchivado innecesario en una compilación sin cambios.

No se midió un nuevo baseline de tiempo: por regla del proyecto, el baseline integral pertenece al
gate final de Alberto. El cierre de esta tanda se basa en invariantes pequeñas, tests unitarios y un
diferencial O0 frío/caliente de una biblioteca real.

## Hallazgos de la auditoría

1. La clave previa a LLVM contenía MIR, toolchain, LLVM, target, nivel de optimización y debug, pero
   no el contrato completo de punto flotante ni el modo LTO. Dos compilaciones con MIR idéntico y
   políticas distintas podían consultar la misma entrada.
2. El objeto se copiaba directamente sobre su nombre final y sólo el sidecar se publicaba mediante
   temporal. Dos procesos podían competir por la misma clave y un lector podía observar una pareja
   objeto/metadata perteneciente a generaciones distintas.
3. Una entrada con checksum incorrecto era un miss seguro, pero permanecía en el almacén y se volvía
   a inspeccionar en cada ejecución.
4. El almacén no tenía una operación de capacidad. No existía una forma soportada de reducirlo sin
   borrar manualmente todo el directorio.
5. Un hit de objetos evitaba el lowering del proyecto, pero una biblioteca estática idéntica volvía
   a ejecutar el archivador. Cuando no había runtime precompilado, también se reconstruía su objeto
   LLVM en cada ejecución.
6. `CompilationWorkerPool` era acotado por `-j`, pero el primer task fatal no descartaba la cola. El
   backend esperaba unidades que ya no podían producir un resultado útil.
7. La ruta O0 rápida ya omitía el pass manager cuando no había `forceinline` obligatorio ni SEH que
   legalizar; faltaba revalidarla junto con los cambios de caché y scheduler.

## Cambios realizados

### Identidad completa de codegen

- Las claves de objeto pasan a schema `NSCOBJKEY 2` y `NSCOBJINPUTKEY 2`.
- Ambas incluyen una identidad canónica de política de backend con:
  - `fp-mode`;
  - `fma-contract`;
  - rounding;
  - tratamiento de denormales;
  - modo LTO.
- La construcción de esa identidad vive en una sola función compartida por generador y linker.
- Se conservan los componentes existentes: identidad única del build del compilador, versión LLVM,
  target/data layout, O-level, debug e identidad MIR/IR versionada.
- Cambiar `precise` por `fast`, o `lto=none` por `lto=full`, produce claves diferentes aun si el MIR
  textual de entrada coincide.

### Publicación concurrente y corrupción

- Cada clave usa un lock de archivo Windows sin sharing. El mismo contrato funciona entre threads y
  entre procesos NSC distintos.
- Objetos y metadata se escriben primero a nombres temporales únicos por proceso/ordinal.
- La publicación usa `MoveFileExW` con replace y write-through mientras se conserva el lock.
- La metadata se invalida antes de reemplazar el payload y se publica al final como marca de commit.
- Restore toma el mismo lock durante lectura, hash y copia; nunca mezcla payload viejo con metadata
  nueva.
- Un sidecar inválido, key incorrecta o checksum/tamaño distinto mueve objeto y metadata a
  `quarantine/`; la entrada deja de participar inmediatamente en futuras consultas.
- Un productor posterior puede reconstruir y publicar limpiamente la misma clave.

### Evicción explícita y segura

- `CompilationArtifactCache::prune` reduce el almacén a un presupuesto de bytes.
- Sólo considera pares publicados y elimina primero metadata, después payload, bajo el lock de la
  clave. Un lector concurrente obtiene hit completo o miss, nunca media entrada.
- El orden es publicación más antigua y luego key canónica para desempate determinista.
- Se añadió `--artifact-cache-max-mb=N`; requiere `--artifact-cache`, acepta 0–1048576 MiB y ejecuta
  un solo barrido por invocación, antes de compilar. No se escanea el store después de cada objeto.
- La consola reporta número de entradas y bytes expulsados.

### No-change antes de runtime LLVM y archivador

- La biblioteca estática construye una identidad final con hashes SHA-256 y tamaños de todos los
  miembros COFF en orden estable.
- Con caché activa, el archivo final verificado se restaura antes de descubrir o ejecutar el
  archivador.
- Si el runtime no viene como biblioteca precompilada, su objeto tiene una clave propia y se
  restaura antes de crear TargetMachine, módulo runtime, optimizador o emisor LLVM.
- Sólo se guardan runtime objects verificados y archives que pasaron `CoffArchiveVerifier`.
- La salida textual distingue `runtime-object-cache` y `static-archive-cache` como
  `hit`, `miss`, `disabled` o runtime externo.
- La compilación fría/caliente final produjo `.lib` completos con el mismo SHA-256; la caliente tuvo
  hit del objeto de proyecto y del archivo final.

### Scheduler cancelable

- `CompilationWorkerPool` publica cancelación, consulta de estado y contador de tasks descartados.
- La primera excepción conserva el error de menor ordinal, marca la ola cancelada y vacía la cola.
- Submissions que compiten con esa cancelación reciben ordinal determinista pero se descartan sin
  abrir trabajo nuevo.
- `wait()` espera únicamente tasks ya activos, reporta el fallo y cierra el epoch de cancelación;
  el pool puede reutilizarse en una ola independiente.
- El generador particionado cancela la cola cuando una unidad devuelve error de lowering,
  optimización, emisión, verificación o caché. Las unidades ya activas terminan de forma segura.
- Excepciones inesperadas de un worker se convierten en diagnóstico LLVM en lugar de escapar del
  driver.

### O0

- No se añadió ningún pipeline a O0.
- Se revalidó el contrato existente de emisión directa preparada.
- `forceinline` obligatorio y legalización SEH siguen siendo las dos excepciones explícitas de
  corrección.
- No se ejecutaron O1, O2 ni O3.

## Archivos nuevos

- `docs/compiler-optimization/TANDA-19-cache-parallel-o0-hardening.md`: este expediente. Se creó
  para que el commit tenga objetivo, hallazgos, protocolo, inventario y evidencia reproducible.

No se creó ningún archivo nuevo de código: los contratos pertenecían a componentes existentes y se
ampliaron en sus ubicaciones naturales.

## Archivos modificados

- `include/nativesharp/Driver/CompilationWorkerPool.h`
- `src/Driver/CompilationWorkerPool.cpp`
- `include/nativesharp/LLVM/CompilationArtifactCache.h`
- `src/LLVM/CompilationArtifactCache.cpp`
- `src/LLVM/ProjectOopObjectGenerator.cpp`
- `include/nativesharp/Linker/WindowsLibraryBuilder.h`
- `src/Linker/WindowsLibraryBuilder.cpp`
- `src/Driver/Driver.cpp`
- `tests/unit/CompilationArtifactCacheTests.cpp`
- `tests/unit/CompilationArtifactCacheDriverTests.cpp`
- `tests/unit/CompilationWorkerPoolTests.cpp`

## Mejora obtenida

- Se elimina una clase de false hit entre políticas FP/LTO.
- Dos NSC simultáneos pueden producir la misma key sin publicar archivos parciales.
- La corrupción deja de convertirse en miss repetitivo y queda aislada para inspección.
- El usuario puede acotar el tamaño sin scripts destructivos externos.
- Un no-change de biblioteca estática evita el frontend LLVM de sus objetos mediante la caché previa
  y evita además el archivador mediante la caché del producto final.
- El runtime interno deja de reconstruirse en LLVM cuando su objeto ya está disponible.
- Un fallo temprano de una partición deja de consumir toda la cola de backend.
- O0 conserva su ruta sin pass manager innecesario.

## Verificación dirigida

### Build

- `nsc` en `A:\nsc-devel\build-clang`, configuración Debug/O0: correcto.
- Sólo se construyeron los ejecutables de prueba nombrados; no se construyó ni ejecutó el target de
  baseline.

### Unitarios

- `nsc.compilation-artifact-cache`: **22/22**.
- `nsc.compilation-artifact-cache-driver`: **5/5**.
- `nsc.compilation-worker-pool`: **11/11**.
- `nsc.compilation-graph-scheduler`: **5/5**.
- `nsc.llvm-optimization`: **129/129**.
- Total de la selección: **172 checks correctos**.

### Diferencial CLI O0

Fixture: `tests/bug_triage/b11_const_field_reference/lib/Limits.nsproj`.

- Compilación fría: exit 0, 1 miss de objeto, miss de archive.
- Compilación caliente idéntica: exit 0, 1 hit de objeto, hit de archive.
- `.lib` fría y caliente: SHA-256 idéntico.
- Capacidad de 64 MiB: segundo barrido reportó 0 expulsiones porque el store estaba bajo límite.
- Todos los temporales, cache y productos del fixture se confinaron a
  `A:\nsc-devel\build-clang\directed-t03` y se eliminaron al finalizar.

## Incidentes de prueba y limpieza

- Un primer intento usó `--emit-project-oop-obj`; esa acción no admite artifact cache. Se descartó.
- Un segundo intento usó un proyecto `OutputType` ejecutable con la acción de biblioteca. Se
  descartó.
- Antes de cachear el archive final se observó que los miembros COFF eran idénticos aunque el
  contenedor producido dos veces no lo era. El hallazgo motivó cachear/restaurar el producto final;
  la comprobación definitiva ya obtuvo `.lib` idénticas.
- El primer probe del nuevo límite reveló que Windows devuelve `path not found` al inspeccionar un
  store todavía inexistente. Ahora se trata como store vacío y está cubierto por el fixture frío.
- Los `error.log` automáticos de los intentos fallidos se eliminaron inmediatamente. No quedó ningún
  directorio `directed-t03` ni `directed-temp-t03`.

## Límites deliberados

- No hubo baseline, benchmark de Math.Core ni corpus completo.
- No hubo pruebas O1/O2/O3.
- No se instaló ni desplegó NSC.
- No se escribió código fuente ni evidencia en `C:`; sólo se ejecutaron las herramientas LLVM ya
  instaladas, con todos los outputs en `A:`.
- La caché final añadida en esta tanda cubre bibliotecas estáticas. EXE conserva su contrato de
  incremental link existente; DLL/import library/PDB se ampliarán en P10 porque requieren una
  transacción multiarchivo, no una copia aislada.
