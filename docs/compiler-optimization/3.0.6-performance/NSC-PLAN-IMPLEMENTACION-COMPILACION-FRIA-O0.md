# NSC — Pendientes y terminados de compilación fría O0

**Progreso:** `31 / 60`

| Ítem | Qué resuelve | Estado |
|---|---|---|
| T1.1 | Evitar que `ColdLlvmTrace` y `tracePhase` abran, escriban y hagan `flush` de archivos cuando no se pidió instrumentación | TERMINADO |
| T1.2 | Mantener un solo flujo bufferizado cuando la instrumentación LLVM sí está habilitada, sin reabrir el archivo por fase | TERMINADO |
| T1.3 | Eliminar la serialización de `combinedExecutableIrText`; `createWindowsImportLibraries` ya ignora ese texto | TERMINADO |
| T1.4 | Convertir `sha256(span)` en cálculo por bloques sin construir una copia acolchada de toda la entrada | TERMINADO |
| T1.5 | Convertir `sha256File` en lectura bufferizada por bloques, sin `istreambuf_iterator` ni vector completo | TERMINADO |
| T1.6 | Leer el PE de salida por tamaño y bloques en `PeExecutableVerifier`, no byte por byte | TERMINADO |
| T1.7 | Leer archivos y salida capturada de NSLink por tamaño y bloques en `LldLinker` | TERMINADO |
| T1.8 | Compartir bytes y SHA-256 de fuentes entre `CompilationInputManifest` y `SourceManager`, evitando la segunda lectura | TERMINADO |
| T1.9 | Reutilizar los digests del manifiesto en `dependencyProvenance` y en la identidad del bundle, evitando volver a hashear las mismas entradas | TERMINADO |
| T2.1 | Calcular una vez `TypeIdentity::fullyQualifiedName` y `TypeIdentity::key` al construir la identidad | TERMINADO |
| T2.2 | Exponer vistas internas de las identidades calculadas y migrar los índices sin cambiar las firmas públicas existentes | TERMINADO |
| T2.3 | Hacer heterogéneas las búsquedas por `string_view` en los índices de tipos, evitando `std::string` temporal por consulta | TERMINADO |
| T2.4 | Hacer heterogéneas las búsquedas de firmas, templates y especializaciones | TERMINADO |
| T2.5 | Reservar capacidad exacta o acotada en los mapas y buckets de `ProjectCompilationIndexes` antes de poblarlos | TERMINADO |
| T2.6 | Añadir el índice global de miembros por nombre requerido por los consumidores que hoy recorren `members()` completo | TERMINADO |
| T2.7 | Añadir índices MIR de layouts de struct y clase por ordinal y por `typeKey` | TERMINADO |
| T2.8 | Añadir índices MIR de funciones por nombre físico y por lambda, además de los índices por miembro ya existentes | TERMINADO |
| T2.9 | Añadir índices directos de planes de asignación, bindings de interfaz y slots de interfaz | TERMINADO |
| T3.1 | Sustituir las tres tablas `unordered_map` de keywords y sus tres consultas por un único despacho estático sin asignaciones | TERMINADO |
| T3.2 | Reservar el vector de tokens con una estimación derivada del tamaño de la fuente | TERMINADO |
| T3.3 | Consultar `knownTypeNames_` con `string_view`, sin construir un `std::string` por identificador | TERMINADO |
| T3.4 | Construir una sola vez el índice inmutable de nombres conocidos y compartirlo entre todos los parsers del corpus | TERMINADO |
| T3.5 | Eliminar las dos materializaciones completas de `knownTypeNames_` en `ExpressionParser` | TERMINADO |
| T3.6 | Convertir `AstArena` en asignación por bloques conservando destrucción tipada y estabilidad de punteros | TERMINADO |
| T3.7 | Detectar antes de `ComptimeEngine` las fuentes que no contienen ninguna forma comptime | TERMINADO |
| T3.8 | Transferir sin copia la fuente sin comptime a `SourceManager` y crear `ComptimeCache` sólo cuando alguna unidad la necesita | TERMINADO |
| T4.1 | Definir un sidecar determinista de productos precompilados ligado al digest de NSMETA y al build-id, sin cambiar NSMETA | TERMINADO |
| T4.2 | Emitir en el productor las superficies pública y de templates que hoy se regeneran desde NSMETA en cada consumidor | TERMINADO |
| T4.3 | Emitir en el productor tokens y AST serializados de ambas superficies para no ejecutar lexer ni parser en frío | TERMINADO |
| T4.4 | Transportar la ruta del sidecar por `LibraryReference`, cierre binario y paquete sin cambiar el contrato de la biblioteca | TERMINADO |
| T4.5 | Validar digest/build-id y restaurar el producto antes de ejecutar `PublicSurfaceBuilder`, lexer o parser | TERMINADO |
| T4.6 | Conservar como fallback exacto la generación vigente cuando el sidecar no exista o no sea válido | TERMINADO |
| T4.7 | Incluir el sidecar en los artefactos de biblioteca y en las reglas CMake de distribución, sin tocar NSC Builder 3 | TERMINADO |
| T5.1 | Resolver `resolveTypeIdentifier` sobre candidatos por nombre, no recorriendo todos los tipos | TERMINADO |
| T5.2 | Resolver una vez los tipos Core conocidos usados por excepciones, `IDisposable`, spans, delegates y conversiones | TERMINADO |
| T5.3 | Sustituir los recorridos globales restantes de tipos en los analizadores de cuerpos por los índices de identidad/nombre | TERMINADO |
| T5.4 | Sustituir los recorridos globales de miembros en `CallAnalyzer` y `ConstructionAnalyzer` por candidatos indexados | TERMINADO |
| T5.5 | Sustituir búsquedas lineales de accessors, predicados de tipo y core calls HIR por índices de miembros | TERMINADO |
| T5.6 | Eliminar los fallbacks lineales restantes de tipos, structs, clases y campos en `ProjectObjectLayoutModel` | TERMINADO |
| T5.7 | Agrupar en `ProjectCompilationIndexes` los locales HIR por miembro propietario | PENDIENTE |
| T5.8 | Agrupar en `ProjectCompilationIndexes` los locales HIR por lambda propietaria | PENDIENTE |
| T5.9 | Agrupar en `ProjectCompilationIndexes` las regiones de cleanup por miembro propietario | PENDIENTE |
| T5.10 | Programar workers HIR sólo para miembros realmente bajables, filtrando abstractos, precompilados y diferidos antes del submit | PENDIENTE |
| T5.11 | Evitar que cada worker HIR de un solo miembro redimensione tablas globales del tamaño de todo el proyecto | PENDIENTE |
| T5.12 | Resolver el creador de cada closure directamente, sin recorrer todas las funciones HIR por lambda | PENDIENTE |
| T5.13 | Sustituir las búsquedas lineales de layouts en `MirNodeMachine` por los índices MIR | PENDIENTE |
| T5.14 | Usar índices directos en MIR para planes de asignación, interfaces, slots e inicializadores estáticos | PENDIENTE |
| T6.1 | Eliminar las comprobaciones duplicadas de builtin y pointer-like en la cadena de `llvmType` | PENDIENTE |
| T6.2 | Memoizar por lowerer los `LLVMTypeRef` exitosos por `typeKey`, sin cachear errores ni cambiar diagnósticos | PENDIENTE |
| T6.3 | Resolver una vez los ordinales SIMD usados por los cinco clasificadores que hoy recorren todos los tipos | PENDIENTE |
| T6.4 | Hacer que `LlvmTypeLayout` consulte los índices MIR de layouts en vez de recorrerlos | PENDIENTE |
| T6.5 | Hacer que `OopLlvmDebugInfo` consulte índices de tipos y miembros | PENDIENTE |
| T6.6 | Hacer que closures, thunks, wrappers y estado estático consulten el índice MIR de funciones | PENDIENTE |
| T6.7 | Reservar mapas y vectores de declaraciones LLVM con los conteos exactos de MIR antes del lowering | PENDIENTE |
| T7.1 | Generar como artefactos del toolchain las import libraries canónicas de KERNEL32, KERNELBASE, SHELL32 y ucrtbase | PENDIENTE |
| T7.2 | Incluir esas import libraries en CMake, paquete e instalación del compilador sin tocar NSC Builder 3 | PENDIENTE |
| T7.3 | Hacer que `WindowsExecutableBuilder` use las import libraries canónicas en lugar de lanzar NSLink cuatro veces | PENDIENTE |
| T7.4 | Limitar `createImportLibrary` durante el build a DLL externas declaradas por el proyecto | PENDIENTE |
| T7.5 | Consultar primero el lock/store ya verificado y resolver/hashear el paquete fuente sólo cuando falte o difiera un artefacto | PENDIENTE |
| T7.6 | Compartir los digests verificados del restore con el manifiesto de compilación y la procedencia de dependencias | PENDIENTE |
