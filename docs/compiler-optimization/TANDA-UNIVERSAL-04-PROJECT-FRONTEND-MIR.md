# Tanda universal 04: proyecto, frontend semántico y MIR acumulativos

## SCOPE LOCK

- Objetivo autorizado: eliminar del modelo de proyecto, del frontend semántico, de los contextos de análisis
  de cuerpos y de la identidad MIR toda decisión funcional basada en `LanguageEdition`.
- Naturaleza: corrección funcional de compatibilidad acumulativa y eliminación de estado obsoleto.
- Invariantes: un proyecto conserva fuentes, referencias, módulos, ABI, ownership, layout, target, arquitectura,
  diagnósticos de sintaxis realmente inválida y resultados de ejecución. `unsafe`, `throws`, `since` y
  `removed` pueden seguir siendo sintaxis reconocida, pero no permisos obligatorios ni fronteras por edición.
- Revisión de entrada: `2a2add0ff`.
- Rango evaluado: desde `2a2add0ff` hasta el commit de implementación de esta tanda.
- Archivos permitidos: proyecto/grafo, frontend semántico, MIR/cache, driver consumidor, pruebas históricas
  directamente afectadas, puente de emisión NSMETA 1.23 y este documento.
- Archivos protegidos: runtime, bibliotecas, bindings, NSLink, NSDBG, NSASM, VS Code y toda unidad `C:`.
- Pruebas históricas: conservan nombres, targets y propósito; sus aserciones por número de edición se sustituyen
  por conducta universal observable.
- Acciones prohibidas: deploy o escritura en `C:`, push remoto, certificación/promoción/instalación de baseline,
  O1/O2/O3, renombrar pruebas históricas, introducir features aplazados o reducir la batería completa O0.
- Terminación de tanda: commit local, reconstrucción limpia del compilador y de los 429 ejecutables de prueba,
  684 CTests, 90 fixtures O0 y 9 casos de bug triage con NSLink; toda falla permanece registrada.

## Causa raíz

La compatibilidad acumulativa estaba simulada, no estructuralmente garantizada. `Project` devolvía siempre V3,
`enforcesLanguage3Effects()` devolvía siempre `false` y el parser ya era acumulativo, pero se seguían propagando
edición y política por proyecto, unidad precompilada, callable, method bodies, MIR y caché. Ese estado muerto
conservaba ramas capaces de volver a exigir `unsafe`, `throws` o disponibilidad por edición y hacía que una
etiqueta administrativa participara en la identidad incremental.

## Cambios de implementación

- `Project` ya no recibe, guarda ni publica edición de origen, reconocida, explícita o inferida.
- `<Language>` continúa siendo aceptado por `ProjectLoader` exclusivamente como entrada histórica ignorada;
  cualquier valor, incluido uno desconocido, no selecciona gramática ni semántica.
- `ProjectGraph` y `PrecompiledProjectDescriptor` identifican módulos por proyecto, metadata y nombre, nunca por
  edición. Normalización, reemplazo de referencias y nodos precompilados ya no reconstruyen ese estado.
- Dumps de proyecto y grafo dejan de publicar una identidad de lenguaje inexistente.
- `PrecompiledSyntaxModule`, `ProjectCompilation` y `ProjectMethodBodyBuildOptions` dejan de propagar edición o
  una matriz de efectos por proyecto.
- `ProjectCallableAnalysisContext` y el scheduler de callables eliminan la edición de cada shard. El resultado
  de una ola no puede variar por una etiqueta de proyecto.
- `ProjectMethodBodyModel` elimina las ramas que hacían obligatorio un bloque `unsafe` para assembly, punteros,
  casts, `assume`, FFI y llamadas marcadas. La sintaxis `unsafe` sigue siendo aceptada, pero no constituye un
  permiso requerido.
- `ProjectMethodBodyModel` elimina la propagación obligatoria de `throws`. `throw`, `try`, `catch`, filtros,
  bindings y flujo de ownership permanecen; una llamada no obliga al consumidor a declarar o capturar el error.
- Se retira el estado muerto de handlers/errores declarados que sólo alimentaba los rechazos anteriores. Un
  `rethrow` conserva en su binding el tipo del `catch` activo.
- `ProjectMetaprogrammingModel` conserva restricciones reales de target `windows`, arquitectura `x64` y avisos
  de deprecación, pero `since` y `removed` ya no rechazan sintaxis que el compilador entiende.
- `ProjectOopMirModel` deja de guardar edición y la identidad de caché sube de `v3` a `v4`; esto invalida de
  forma determinista entradas antiguas sin convertir una versión de lenguaje en parte de la clave.
- La salida humana y JSON de `nsc build` deja de publicar edición. La guía de migración declara un lenguaje
  fuente acumulativo.
- NSMETA 1.23 todavía reserva un byte histórico en su formato binario. En esta tanda se desacopla su emisión de
  `Project`: se escribe el valor legado compatible y se publican siempre contratos de lifetime. La eliminación
  completa del campo, validadores y consumidores NSMETA pertenece a la siguiente tanda y no puede volver a
  influir en proyecto, parser, frontend o MIR.

## Pruebas históricas adaptadas sin renombrar

- `LanguageEditionInferenceTests`: comprueba que proyecto omitido, dependencias etiquetadas y grafos mixtos
  conservan módulos y fuentes.
- `ImplicitLanguageEditionSemanticsTests`: comprueba que una llamada a una función declarada `unsafe` no exige
  un permiso en el consumidor y que ambos módulos permanecen en el grafo.
- `ImplicitLanguageFeatureRecognitionTests`: comprueba `copy struct` bajo proyecto sin etiqueta, con `2` y con
  `3`, sin seleccionar semántica distinta.
- `PrecompiledLanguageEditionInferenceTests`: comprueba composición de módulos precompilados sólo por identidad
  física y nombre.
- `NativeSharp3T00ConstitutionTests`: conserva el nombre histórico y valida que el loader, dump, grafo, frontend
  y NSMETA operen sin una frontera de edición.
- `NativeSharp3T12AvailabilityTests`: `since` y `removed` son metadata informativa; target y arquitectura siguen
  siendo restricciones funcionales.
- `Item403MigrationGuideTests`: comprueba la declaración acumulativa de la guía.

## Archivos modificados

- `include/nativesharp/Basic/Version.h`
- `include/nativesharp/Compilation/Project.h`
- `include/nativesharp/Compilation/ProjectGraph.h`
- `include/nativesharp/Frontend/ProjectCallableAnalysisContext.h`
- `include/nativesharp/Frontend/ProjectCompilation.h`
- `include/nativesharp/Frontend/ProjectMetaprogrammingModel.h`
- `include/nativesharp/Frontend/ProjectMethodBodyBuildOptions.h`
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
- `include/nativesharp/Frontend/ProjectOopMirModel.h`
- `src/Compilation/Project.cpp`
- `src/Compilation/ProjectDumper.cpp`
- `src/Compilation/ProjectGraph.cpp`
- `src/Compilation/ProjectGraphDumper.cpp`
- `src/Compilation/ProjectGraphLoader.cpp`
- `src/Compilation/ProjectLoader.cpp`
- `src/Driver/Driver.cpp`
- `src/Driver/MigrationGuide.cpp`
- `src/Driver/ProjectCommands.cpp`
- `src/Frontend/ProjectCallableAnalysisContext.cpp`
- `src/Frontend/ProjectCompilation.cpp`
- `src/Frontend/ProjectMetaprogrammingModel.cpp`
- `src/Frontend/ProjectMethodBodyModel.cpp`
- `src/Frontend/ProjectOopMirModel.cpp`
- `src/Interop/LibraryMetadata.cpp`
- `src/LLVM/ProjectOopMirCacheIdentity.cpp`
- `tests/unit/ImplicitLanguageEditionSemanticsTests.cpp`
- `tests/unit/ImplicitLanguageFeatureRecognitionTests.cpp`
- `tests/unit/Item403MigrationGuideTests.cpp`
- `tests/unit/LanguageEditionInferenceTests.cpp`
- `tests/unit/NativeSharp3T00ConstitutionTests.cpp`
- `tests/unit/NativeSharp3T12AvailabilityTests.cpp`
- `tests/unit/PrecompiledLanguageEditionInferenceTests.cpp`

## Complejidad y rendimiento

- No se introduce recursión.
- Se eliminan vectores por proyecto, campos por callable, ramas por expresión y llamadas no-op por invocación.
- La programación de olas conserva su complejidad; cada callable transporta menos estado y cada operación nativa
  evita comprobaciones de permisos que nunca debieron decidir su validez.
- La invalidación `v4` evita reutilizar una caché cuya identidad incluía la antigua edición.

## Verificación previa al commit

- `git diff --check`: sin errores de whitespace.
- Accesores eliminados de `Project`: cero consumidores en `include`, `src` y `tests`.
- Estado eliminado de method bodies: cero consumidores de `unsafeBlockDepth_`, matrices de edición, handlers de
  propagación obligatoria y `validateErrorPropagation`.
- Constructores de `PrecompiledProjectDescriptor`: todos ajustados al contrato de tres campos.
- Archivo temporal de auditoría: eliminado antes del commit.
- Pruebas todavía no ejecutadas: la metodología obliga a commitear la implementación antes de construir.

## Verificación posterior al commit

Pendiente de la batería completa O0 con NSLink. La evidencia se agregará en un commit posterior sin reescribir
el resultado ni declarar cierre si existe una sola prueba fallida, omitida o no ejecutada.
