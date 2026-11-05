# Etapa B: auditoría de frontera C++ y NativeSharp

Estado: EN ANÁLISIS. Esta nota conserva hallazgos iniciales; no cierra la etapa ni declara interoperabilidad completa. Fuentes de NSC, NSLink, bindings y toolchain de referencia consultadas en solo lectura. Daikon queda fuera por instrucción del usuario.

La referencia local de la rama `development-cpp-clang` de NSC apunta a `152654e5a0e2d84d4c2f245ea7590488f0497430`; esta lectura no verifica que el árbol de trabajo esté limpio ni identifica el binario instalado con ese commit.

## Preguntas prioritarias

### 1. Demanda y recuperación

**Parcial; no demostrado para demanda C++.** `include/nativesharp/LanguageService/LanguageService.h` ofrece `analyzeProject` y consultas para editor. `AnalysisSnapshot.h` expone diagnósticos, símbolos y la disponibilidad del modelo, pero la interfaz consultada no ofrece una operación para entregar una demanda de template C++ con sus argumentos ya resueltos y reanudar semántica. La ausencia de ese método en esta fachada no demuestra que un conductor externo no pueda utilizar otra interfaz o reconstruir el proyecto. Falta seguir el driver, el diagnóstico, la recuperación y los modelos internos hasta comprobar la cadena completa.

### 2. Representación externa

**Parcial; no demostrada para C++ general.** `LibraryMetadataType.h` conserva tamaño, alineación, campos, identidad y `dropGlueEntryName`; `LibraryMetadataMember.h` conserva firma, nombres C y mangled, parámetros y política de unwind. `src/Driver/PublicSurfaceBuilder.cpp` emite superficies con `pack`, `align`, `size` y offsets, y `src/Frontend/HIR/HirBuildCoordinator.cpp` utiliza `precompiledMangledName` al importar miembros. Esto refuta una negación general de que metadata pueda llevar layout o nombres arbitrarios. No demuestra que acepte layout C++ arbitrario, bases virtuales, vtables, constructores, destrucción, excepciones o una metadata fabricada externamente que respete todas las verificaciones. Falta recorrer validadores, reconstrucción de tipos, símbolos, codegen y ejecución.

Límite concreto: `src/Interop/LibraryMetadataVerification.cpp:140` exige que el drop glue de un struct use el nombre `__nsc_drop_value_` derivado de su identidad, y la línea 150 exige ese contrato para campos `owned`. Un destructor C++ con cualquier otro nombre no se puede poner directamente como `dropGlueEntryName` en esa metadata. Queda por investigar si un símbolo adaptador generado con el nombre exigido preserva el comportamiento sin modificar NSC; este hallazgo por sí solo no prueba que cambiar NSC sea indispensable.

### 3. Genérico NativeSharp con argumento C++

**Parcial; no demostrado para tipo C++.** `LibraryMetadataGenericTemplate.h` conserva `sourceText`, dependencias y contratos. `src/Driver/PrecompiledSurface/PrecompiledSourceGeneration.cpp` incorpora ese texto a fuente sintética; `src/Frontend/Generics/TypeDemandScheduler.cpp` y `SpecializationExecutor.cpp` materializan especializaciones durante el proceso del compilador. No se ha encontrado ni descartado una API aislada que acepte identidad y semántica C++ y devuelva el objeto resultante. El mecanismo existente demuestra especialización NS, no interoperabilidad cruzada.

`ProjectGenericModel.h` expone `build` sobre un grafo de proyecto y unidades sintácticas, junto con consultas de especializaciones; sus planificadores y operaciones de materialización son privados. Su función pública `closedTypeKeyForTemplate` calcula una clave para un caso documentado; no equivale a compilar un genérico con un tipo C++ externo. Es posible que un proyecto sintético o una interfaz diferente resuelva el problema, aún sin verificar.

### 4. COMDAT y compilación separada

**Implementaciones y pruebas presentes; cruce mixto no verificado.** NSC usa `LLVMSetComdat` y selección `Any` en `src/LLVM/Lowering/Support/DispatchGlobals.cpp`; `tests/unit/Stage44GenericsMonomorphizationTests.cpp` comprueba presencia de COMDAT en LLVM IR. El proyecto separado `A:/nslink-devel` contiene `src/link/ComdatResolver.cpp`, con selección `Any`, `SameSize`, `ExactMatch`, `Largest` y secciones asociativas, y `tests/link/ComdatTests.cpp` contiene pruebas correspondientes. Es evidencia de código y pruebas existentes, no ejecución en esta auditoría ni prueba de que NSLink y LLD resuelvan todas las combinaciones de objetos mixtos, archivos y DLL. No atribuir NSL1021 a COMDAT sin reproducción.

## Puntos de integración de Clang

`clang/include/clang/AST/ExternalASTSource.h` ofrece declaración y consulta de layouts externos, incluido `layoutRecordType`; `clang/include/clang/Sema/ExternalSemaSource.h` añade una interfaz semántica. Son candidatos de integración, no demuestran por sí mismos generación correcta de clases NativeSharp, plantillas, herencia, RTTI, excepciones o ownership. El frontend Clang debe continuar resolviendo C++ con su semántica completa; el intercambio de información tiene que incorporar identidades, demanda de instancias, dependencias y diagnósticos en ambas direcciones.

`A:/nsc-bindings-devel/bindings/LANGUAGE_SURFACE.md` documenta importación estática y dinámica mediante `extern C`. Es antecedente de generación de superficies, no prueba de importación semántica de C++ general ni autorización para limitar el alcance a C.

`A:/nsc-bindings-devel/bindings/nsbind/nsbind.py:4` describe extracción de headers C con libclang a partir de una especificación; su configuración de libclang empieza en la línea 129. Es evidencia de infraestructura reutilizable para lectura de headers y generación, no de coordinación abierta de templates C++ con argumentos NS.

## Diseño pendiente

- Determinar si el conductor puede producir metadata NS válida y recuperar demandas sin modificar NSC, incluidas identidades de tipos importados y especialización genérica recíproca. Si se demuestra que alguna pregunta requiere modificar NSC, notificar inmediatamente antes de proseguir con esa decisión.
- Distinguir valor trivial, valor con recursos y clase NS por referencia; definir construcción, copia, movimiento, destrucción, identidad y paso ABI en cada dirección.
- Definir un protocolo de excepciones y cleanup que respete unwind y los contratos de ambos lenguajes; la metadata enumera una política FFI, pero no demuestra que una excepción C++ pueda atravesar frames NS.
- Determinar ownership por módulo, allocator, drop glue, instancias de runtime, DLL y manejo de inicializadores/TLS. No asumir que compartir target o heap implica compartir estado.
- Construir el inventario reproducible de contratos y sus fuentes; después definir la arquitectura sustentada en evidencia. No presentar estas posibilidades como implementación existente.

### Alternativa de trabajo a contrastar, no decisión cerrada

Un conductor propiedad del toolchain podría mantener sesiones semánticas de Clang y NSC, pedir a Clang la resolución y la especialización C++ para cada configuración real de compilación, y transportar a NSC las identidades y los contratos que su metadata admita. En la dirección inversa, podría publicar en Clang declaraciones obtenidas de metadata NS y solicitar especializaciones NS mediante un proyecto sintético si el compilador lo permite. El conductor tendría que detectar ciclos de demanda, versionar cachés con fuentes/flags/toolchain y producir símbolos adaptadores cuando el contrato los requiera, sin pedir al usuario que los escriba. No se ha demostrado que esta coordinación pueda cubrir toda la semántica sin cambios de NSC; investigar esa posibilidad es parte del objetivo, no un supuesto.

La frontera física debería registrar por entidad origen, identidad estable, layout y clasificación de parámetros/retorno; por operación, símbolos, construcción, copia, movimiento, destrucción y garantías de unwind; por módulo, allocator, estado de runtime y ciclo de vida. Cualquier puente de errores deberá preservar el comportamiento observable requerido y cleanup de cada lenguaje; una conversión o un rechazo que pierda comportamiento se registra como capacidad pendiente, no como solución completa.

### Evidencia inicial para excepciones

`src/LLVM/Lowering/OopLlvmCleanup.cpp` contiene rutas de limpieza excepcional y `buildPotentiallyThrowingCall`; `OopLlvmThunksAndWrappers.cpp:737` describe que el wrapper `export extern C` intercepta excepciones NativeSharp que intenten salir, mientras deja propagar una excepción SEH extranjera según el comentario de implementación. Por tanto, no se puede asumir que exportar una función como C proporcione el comportamiento C++ solicitado para excepciones NS. El diseño debe rastrear el código y las pruebas de cada dirección, no extrapolar ese comentario a todas las fronteras. La política final sigue abierta.

## Límites de esta nota

No se han ejecutado pruebas de interoperabilidad nuevas, no se han inspeccionado todas las rutas de los cuatro interrogantes, no se ha validado cada capacidad de NSLink/LLD, no hay decisión cerrada de arquitectura ni inventario completo. La etapa B continúa abierta.
