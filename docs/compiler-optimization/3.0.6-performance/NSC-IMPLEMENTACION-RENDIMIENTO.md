# NSC — Implementación y tracking absoluto de rendimiento de compilación

**Fecha original:** 2026-10-20
**Reauditoría vigente:** 2026-10-21, `development` en `aa6644dde7418ea267d1006f251863dbe382505b`
**Documento base:** `NSC-ANALISIS-RENDIMIENTO-POR-FUNCION.md`, en la misma carpeta. Ahí están las ediciones exactas; este documento las ordena, les pone gate y les asigna autorización.
**Rama de trabajo:** `development`. El respaldo certificado es la versión 3.0.6.

**Reglas aplicadas.** Las de `A:\LINEAMIENTOS-DESARROLLO-COMPILADOR-NATIVESHARP.md`, leído completo hoy, y las secciones de `A:\LINEAMIENTOS-DE-TRABAJO-Y-PORTEO.md` que aplican a optimizaciones:

- §2 presupuesto de cambio, un commit por hipótesis y gate de diferencial;
- §8 conservación en optimizaciones;
- §9 tamaño, estilo y comentarios obligatorios;
- §10 complejidad y prohibición de recursión.

**Certeza.** HECHO = leído en el código o en las pruebas con línea exacta. NO VERIFICADO = no se pudo comprobar con lo disponible.

**Propiedad y alcance vigentes.** Toda la implementación de los 302 work items atómicos y de sus 60 paquetes
causales será realizada por Codex sobre este repositorio. El trabajo del IDE queda pausado y fuera de esta
campaña: no se delega ninguna optimización al agente del IDE ni se depende de él para cerrar una tanda. Se
conservan las APIs que el compilador ya expone al IDE. Cualquier solicitud futura para exponer una API nueva
se tratará como un alcance separado y no entrará silenciosamente en este tracking.

## 0. Control absoluto y distribución

**Estado absoluto de implementación:** `0 / 302` work items atómicos terminados.
**Paquetes causales:** `0 / 60` entradas `T-xx` cerradas.
**Tandas:** `0 / 7` cerradas.

| Tanda | Entradas | Paquetes | Work items | Alcance dominante |
|---|---|---:|---:|---|
| 1 | `T-01..T-05`, `T-25..T-27` | 8 | 20 | Identidades, SHA-256, PE, LLVM y driver |
| 2 | `T-06..T-19` | 14 | 84 | Índices internos, fallbacks y caché de tipos LLVM |
| 3 | `T-20..T-24` | 5 | 30 | HIR, orden paralelo, bindings y closures |
| 4 | `T-29` | 1 | 9 | Carga y normalización del grafo de proyectos |
| 5 | `T-30`, `T-31`, `T-62`, `T-33..T-40` | 11 | 28 | Genéricos, lexer, parser y nombres conocidos |
| 6 | `T-41..T-54` | 14 | 100 | Superficie, metadatos, bodies e identidades precompiladas |
| 7 | `T-55..T-61` | 7 | 31 | Debug info, MIR, visibilidad, estáticos, referencias y LLVM |
| **Total** |  | **60** | **302** | **Implementación completa del Grupo A** |

Los **11 cambios del Grupo B no forman parte del denominador `0/302`** y no se ejecutan sin autorización
explícita de Alberto. Los **12 hallazgos del Grupo C** permanecen documentados como `NO IMPLEMENTAR`.

No se inicia `T-01` hasta que Alberto revise este documento y autorice expresamente el comienzo. Antes del
primer commit también debe existir el oráculo de artefactos descrito en la sección 1.

### 0.1 Reauditoría del código y de los commits posteriores

**HECHO.** La rama es `development`. El `HEAD` al iniciar la revisión era
`1bded1486299a90b0b956e3029ce60b8796c989d` y el worktree estaba limpio. Durante la revisión, Alberto
incorporó `aa6644dde7418ea267d1006f251863dbe382505b`, limitado a este documento; ese commit también fue
auditado y es el `HEAD` vigente de este cierre.

Se revisaron todos los commits posteriores al estado certificado `8eef25d97`:

- `3c8df0287`: añadió semantic member completion y extrajo la preparación precompilada a
  `Compilation/PrecompiledProjectPreparation.cpp`;
- `acab683ef` y `f78fd562a`: incorporaron el análisis y el tracking de rendimiento originales;
- `ffbae3dfb`: retiró una consulta duplicada de miembros promovidos;
- `387c579f2`: corrigió el recorrido de la raíz de namespace del servicio de lenguaje;
- `dd2992e89`: documentó su validación;
- `1bded1486`: reutilizó el snapshot residente para completion;
- `aa6644dde`: actualizó este tracking con la reauditoría y el denominador granular; no modificó código
  del compilador.

**Conclusión de la reauditoría.** Ninguno de esos commits cerró una entrada `T-xx` del camino de
`nsc build`. El cambio `1bded1486` optimiza exclusivamente la consulta interactiva del servicio de
lenguaje y no se vuelve a contar aquí. Los solapamientos reales con este plan son:

- `T-27` se mueve de `Driver/PrecompiledModuleConfiguration.cpp` a
  `Compilation/PrecompiledProjectPreparation.cpp::loadExplicitMetadata`;
- `T-57` debe conservar las nuevas rutas `typeIsVisibleFromProject` y
  `nestedTypeIsVisibleFrom` al precalcular nombres plegados;
- `T-47` conserva el nuevo acceso público `ProjectMemberModel::typeOrdinalForTypeKey`; no sustituye
  el índice pendiente `membersDeclaredByType`;
- `T-51` y `T-54` deben convivir con `completionScopes_`, que no sustituye el índice de extensiones
  ni el índice de identidades precompiladas;
- `TransparentStringHash` ya existe anidado en `ProjectMemberModel`; `T-06` debe extraerlo a un
  header compartido y reutilizarlo, no introducir una segunda implementación.

La vista canónica de estado es `NSC-PENDIENTES-Y-TERMINADOS-RENDIMIENTO.md`: contiene los 302 work items
atómicos, sin agrupar consumidores ni sitios. Los bullets técnicos siguientes son una descomposición de diseño
en 174 pasos y no son el denominador de avance. No son 302 commits: un commit sigue correspondiendo a una
entrada `T-xx` y puede cerrar varios work items inseparables de la misma hipótesis.

### 0.2 Descomposición técnica por tandas — 174 pasos de diseño

Cada bullet hereda el presupuesto, dependencia, gate y prueba focal de su entrada `T-xx` en la sección 6.
Cambiar su archivo o símbolo exige actualizar primero el presupuesto documental.

#### Tanda 1 — `0 / 16`

- [ ] `PERF-0001` (`T-01`) Añadir a `TypeIdentity` el valor residente de `fullyQualifiedName`.
- [ ] `PERF-0002` (`T-01`) Añadir a `TypeIdentity` el valor residente de `key` sin reconstruirlo por llamada.
- [ ] `PERF-0003` (`T-01`) Hacer que ambos accesores devuelvan referencias estables sin cambiar su texto.
- [ ] `PERF-0004` (`T-01`) Añadir `TypeIdentityCacheTests.cpp` con copia, namespace, anidado y aridad.
- [ ] `PERF-0005` (`T-02`) Extraer `sha256BlockWords` conservando literalmente la transformación actual.
- [ ] `PERF-0006` (`T-02`) Rehacer `sha256` por bloques sin copiar y rellenar toda la entrada.
- [ ] `PERF-0007` (`T-03`) Leer `sha256File` en bloques de 64 KiB sin lectura byte a byte.
- [ ] `PERF-0008` (`T-02`) Añadir equivalencia en memoria para tamaños `0..300`, 1 MiB y vectores FIPS.
- [ ] `PERF-0009` (`T-03`) Añadir equivalencia de archivo para los seis tamaños frontera del tracking.
- [ ] `PERF-0010` (`T-04`) Sustituir `readPeBytes` por una lectura contigua preservando errores y contenido.
- [ ] `PERF-0011` (`T-05`) Retirar de `llvmType` el chequeo de puntero duplicado antes de `mapLlvmType`.
- [ ] `PERF-0012` (`T-25`) Calcular `dependencyProvenance` solo cuando se solicitó `--dependency-report`.
- [ ] `PERF-0013` (`T-26`) Canonicalizar una vez la raíz por paquete en `appendArtifact`.
- [ ] `PERF-0014` (`T-26`) Mover, no copiar, el plan al cierre de `executeRestore`.
- [ ] `PERF-0015` (`T-27`) Deduplicar rutas de NSMETA en
  `Compilation/PrecompiledProjectPreparation.cpp::loadExplicitMetadata`.
- [ ] `PERF-0016` (`T-27`) Añadir `PrecompiledMetadataDedupTests.cpp` y registrar su target focal.

#### Tanda 2 — `0 / 48`

- [ ] `PERF-0017` (`T-06`) Extraer `TransparentStringHash` de `ProjectMemberModel` a su header compartido.
- [ ] `PERF-0018` (`T-06`) Migrar los mapas actuales de `ProjectMemberModel` al hash compartido sin cambiar tipos.
- [ ] `PERF-0019` (`T-06`) Declarar las consultas por identity key y type key en `ProjectNameLookup`.
- [ ] `PERF-0020` (`T-06`) Añadir el mapa de ordinal por identity key y su bandera independiente de `built_`.
- [ ] `PERF-0021` (`T-06`) Construir el índice una sola vez dentro de `ProjectNameLookup::build`.
- [ ] `PERF-0022` (`T-06`) Conservar el recorrido lineal original cuando la bandera del índice sea falsa.
- [ ] `PERF-0023` (`T-06`) Registrar `NameLookup/TypeIdentityKeyIndex.cpp` en CMake.
- [ ] `PERF-0024` (`T-06`) Añadir la prueba diferencial completa, incluido el modelo armado manualmente.
- [ ] `PERF-0025` (`T-07`) Delegar `ProjectInheritanceModel::typeOrdinalForTypeKey` al índice común.
- [ ] `PERF-0026` (`T-07`) Delegar `ProjectInterfaceModel::typeOrdinalForTypeKey` al índice común.
- [ ] `PERF-0027` (`T-07`) Delegar `ProjectGenericConstraintModel::declarationForTypeKey` al índice común.
- [ ] `PERF-0028` (`T-08`) Indexar `ProjectObjectLayoutModel::valueLayoutForTypeKey`.
- [ ] `PERF-0029` (`T-08`) Indexar `ProjectObjectLayoutModel::hasTrivialStructLifetime`.
- [ ] `PERF-0030` (`T-08`) Indexar `ProjectObjectLayoutModel::structBitFieldWidth`.
- [ ] `PERF-0031` (`T-08`) Indexar `ProjectObjectLayoutModel::isClassBitFieldBase`.
- [ ] `PERF-0032` (`T-09`) Sustituir las búsquedas de tipo en `HIR/HirBuildCoordinator.cpp`.
- [ ] `PERF-0033` (`T-09`) Sustituir la búsqueda de enum en `HIR/HirExpressionMachine.cpp`.
- [ ] `PERF-0034` (`T-09`) Sustituir la búsqueda de tipo en `HIR/HirCallLowering.cpp`.
- [ ] `PERF-0035` (`T-09`) Sustituir la búsqueda en `HirCoreCallFrameLowering.cpp`.
- [ ] `PERF-0036` (`T-10`) Sustituir la búsqueda de tipo de `MIR/Internal/MirNodeMachine.cpp`.
- [ ] `PERF-0037` (`T-11`) Migrar las dos consultas de `Bodies/MethodBodyTypeQueries.cpp`.
- [ ] `PERF-0038` (`T-11`) Migrar las tres consultas de `Bodies/TypeKeyPredicates.cpp`.
- [ ] `PERF-0039` (`T-11`) Migrar las seis consultas de `Bodies/ConstructionAnalyzer.cpp`.
- [ ] `PERF-0040` (`T-11`) Migrar `Bodies/LambdaDelegateBinder.cpp`.
- [ ] `PERF-0041` (`T-11`) Migrar las dos consultas de `Bodies/ConversionAndPassKindChecks.cpp`.
- [ ] `PERF-0042` (`T-12`) Migrar `MemberModel/BuildAndDeclaration.cpp::namedTypeForKey`.
- [ ] `PERF-0043` (`T-13`) Migrar `LLVM/LlvmTypeLayout.cpp::findEnumUnderlyingInfo`.
- [ ] `PERF-0044` (`T-13`) Migrar las consultas de `LLVM/LlvmTypeClassification.cpp`.
- [ ] `PERF-0045` (`T-13`) Migrar `LLVM/Lowering/OopLlvmDebugInfo.cpp::debugNamedType`.
- [ ] `PERF-0046` (`T-13`) Migrar las dos consultas de `OopLlvmThunksAndWrappers.cpp`.
- [ ] `PERF-0047` (`T-13`) Migrar las cinco consultas SIMD/ABI de `OopLlvmTypesAbiAttributes.cpp`.
- [ ] `PERF-0048` (`T-14`) Migrar las dos consultas de tipos de `Interop/CHeaderGenerator.cpp`.
- [ ] `PERF-0049` (`T-15`) Añadir almacenamiento y bandera del índice `fieldLayoutByMember`.
- [ ] `PERF-0050` (`T-15`) Construir el índice con precedencia struct, class y promovido.
- [ ] `PERF-0051` (`T-15`) Hacer que `fieldLayoutForMember` use índice o fallback lineal idéntico.
- [ ] `PERF-0052` (`T-15`) Añadir y registrar `FieldLayoutIndexTests.cpp`.
- [ ] `PERF-0053` (`T-16`) Añadir mapas y accesores de layout MIR por `typeKey`.
- [ ] `PERF-0054` (`T-16`) Construir ambos índices antes de bajar funciones MIR.
- [ ] `PERF-0055` (`T-16`) Delegar `findStructLayout` y `findClassLayout` desde LLVM.
- [ ] `PERF-0056` (`T-16`) Añadir y registrar `MirLayoutIndexTests.cpp`.
- [ ] `PERF-0057` (`T-17`) Añadir la sobrecarga indexada de `abiPassingForType`.
- [ ] `PERF-0058` (`T-17`) Pasar el índice a `configureFunctionAbi` y `lowerOneFunction`.
- [ ] `PERF-0059` (`T-17`) Usar el índice en `sumAwaitedFrameSizeNodes`.
- [ ] `PERF-0060` (`T-18`) Añadir `sharedStructLayouts_` y `activeStructLayouts` sin retirar el vector actual.
- [ ] `PERF-0061` (`T-18`) Compartir las layouts con cada lowerer de función en vez de copiarlas.
- [ ] `PERF-0062` (`T-18`) Migrar los dos lectores de `MirNodeMachine` al acceso activo.
- [ ] `PERF-0063` (`T-19`) Añadir `llvmTypeCache_` a `OopLlvmImplementation`.
- [ ] `PERF-0064` (`T-19`) Memoizar solamente resultados LLVM no nulos y conservar fallos idénticos.

#### Tanda 3 — `0 / 18`

- [ ] `PERF-0065` (`T-20`) Extraer `makePrecompiledRegularFunction` sin mutar el modelo HIR.
- [ ] `PERF-0066` (`T-20`) Extraer `makeImportedCRegularFunction` sin mutar el modelo HIR.
- [ ] `PERF-0067` (`T-20`) Añadir `resolveRegularFunctionWithoutBody` para importadas y descartadas.
- [ ] `PERF-0068` (`T-20`) Resolver esas funciones en el coordinador sin crear un worker completo.
- [ ] `PERF-0069` (`T-21`) Centralizar las tres escrituras en `recordFunctionForMember`.
- [ ] `PERF-0070` (`T-21`) Evitar que cada worker dimensione los vectores globales de función.
- [ ] `PERF-0071` (`T-22`) Hacer directa la ruta filtrada de `buildRegularFunctionsSequentially`.
- [ ] `PERF-0072` (`T-23`) Añadir el índice `localOrdinalsByCallable`.
- [ ] `PERF-0073` (`T-23`) Añadir el índice `localOrdinalsByLambda`.
- [ ] `PERF-0074` (`T-23`) Añadir el índice `parameterCleanupOrdinalsByCallable`.
- [ ] `PERF-0075` (`T-23`) Construir los tres índices una sola vez en `indexBuildBindings`.
- [ ] `PERF-0076` (`T-23`) Usar locales indexados al construir funciones regulares.
- [ ] `PERF-0077` (`T-23`) Usar locales indexados al construir funciones de namespace.
- [ ] `PERF-0078` (`T-23`) Usar locales indexados al construir funciones lambda.
- [ ] `PERF-0079` (`T-23`) Usar cleanups indexados en funciones regulares.
- [ ] `PERF-0080` (`T-23`) Usar cleanups indexados en funciones de namespace.
- [ ] `PERF-0081` (`T-20`) Añadir `HirParallelOrderTests.cpp` para comparar `-j1` contra `-jN`.
- [ ] `PERF-0082` (`T-24`) Indexar una vez `functionByLambdaOrdinal` en `markLambdaClosureCells`.

#### Tanda 4 — `0 / 4`

- [ ] `PERF-0083` (`T-29`) Evitar canonicalizaciones repetidas en `ProjectSourceSet::add`.
- [ ] `PERF-0084` (`T-29`) Reutilizar índices y claves durante `ProjectSourceSet::finalize`.
- [ ] `PERF-0085` (`T-29`) Indexar `normalizeTransitiveSourceOwnership` sin alterar el orden del grafo.
- [ ] `PERF-0086` (`T-29`) Indexar `preferExactSourceModuleArtifacts` conservando el primer artefacto.

#### Tanda 5 — `0 / 23`

- [ ] `PERF-0087` (`T-30`) Añadir `declaredTypeOrdinalsBySourceName_` a `ProjectGenericModel`.
- [ ] `PERF-0088` (`T-30`) Poblar el índice durante `TemplateCollector` en orden de ordinal.
- [ ] `PERF-0089` (`T-30`) Usarlo en `resolveModuleQualifiedDeclaredTypeIdentity`.
- [ ] `PERF-0090` (`T-30`) Usarlo en `resolveDeclaredTypeIdentity`.
- [ ] `PERF-0091` (`T-30`) Añadir y registrar `DeclaredTypeIdentityIndexTests.cpp`.
- [ ] `PERF-0092` (`T-31`) Hacer perezosa la materialización de `GenericLookupScope::Cow`.
- [ ] `PERF-0093` (`T-62`) Evitar materializar `Cow` cuando `GenericNodeProcessor` no escribe.
- [ ] `PERF-0094` (`T-33`) Evitar copiar `children()` en el sitio seguro de `ProjectGenericModel.cpp`.
- [ ] `PERF-0095` (`T-33`) Evitar copiar `children()` en el sitio seguro de `TypeAliasExpander.cpp`.
- [ ] `PERF-0096` (`T-33`) Evitar copiar `children()` en el sitio seguro de `GenericNodeProcessor.cpp`.
- [ ] `PERF-0097` (`T-33`) Auditar y eliminar la primera copia segura en `TemplateDependencyCollector.cpp`.
- [ ] `PERF-0098` (`T-33`) Auditar y eliminar la segunda copia segura en `TemplateDependencyCollector.cpp`.
- [ ] `PERF-0099` (`T-33`) Evitar copiar `children()` en el sitio seguro de `TemplateCollector.cpp`.
- [ ] `PERF-0100` (`T-34`) Evitar la copia de `originNamespace()` conservando lifetime y nulabilidad.
- [ ] `PERF-0101` (`T-35`) Posponer strings en `TemplateResolver::resolveTypeTemplate` hasta necesitarlos.
- [ ] `PERF-0102` (`T-35`) Reutilizar la resolución en `TypeDemandScheduler::processTypeSyntax`.
- [ ] `PERF-0103` (`T-36`) Precalcular por proyecto la ruta canónica usada por `buildVisibility`.
- [ ] `PERF-0104` (`T-37`) Resolver `keywordKind` con una sola consulta discriminada.
- [ ] `PERF-0105` (`T-38`) Reservar capacidad estimada en `Lexer::lexAll`.
- [ ] `PERF-0106` (`T-39`) Añadir `typeNameLookupScratch_` al parser sin cambiar `knownTypeNames_`.
- [ ] `PERF-0107` (`T-39`) Reservar el scratch durante la construcción del contexto del parser.
- [ ] `PERF-0108` (`T-39`) Reutilizar el scratch en la consulta de `TypeParser.cpp`.
- [ ] `PERF-0109` (`T-40`) Reservar y construir una sola vez la lista de nombres conocidos por compilación.

#### Tanda 6 — `0 / 50`

- [ ] `PERF-0110` (`T-41`) Añadir `findMetadataTypeByTypeKey` con ruta ordenada y fallback seguro.
- [ ] `PERF-0111` (`T-41`) Usar la consulta en `PublicSurfaceBuilder::collectPublishedMembers`.
- [ ] `PERF-0112` (`T-41`) Usar la consulta en `PublicSurfaceBuilder::publishContractTypes`.
- [ ] `PERF-0113` (`T-41`) Añadir `PublicSurfaceLookupTests.cpp` para metadata ordenada y en memoria.
- [ ] `PERF-0114` (`T-42`) Calcular una vez el rank usado por `orderedPublishedMethods`.
- [ ] `PERF-0115` (`T-42`) Evitar llamadas repetidas a `dispatchRank` dentro del comparador.
- [ ] `PERF-0116` (`T-43`) Crear `ContractMaterializationCache` en archivos propios.
- [ ] `PERF-0117` (`T-43`) Consultar la caché en los sitios de materialización de contratos.
- [ ] `PERF-0118` (`T-43`) Propagar una sola caché por toda la materialización recursiva del módulo.
- [ ] `PERF-0119` (`T-43`) Añadir y registrar `ContractMaterializationCacheTests.cpp`.
- [ ] `PERF-0120` (`T-44`) Memoizar estados de `verifyGenericTemplateClosure`.
- [ ] `PERF-0121` (`T-44`) Compartir el memo entre raíces desde `verifyGenericTemplates`.
- [ ] `PERF-0122` (`T-45`) Indexar las tablas de dispatch de metadata por identidad.
- [ ] `PERF-0123` (`T-45`) Indexar miembros virtuales usados por `verifyVirtualMemberDispatch`.
- [ ] `PERF-0124` (`T-45`) Sustituir los recorridos `findMetadataDispatch*` por los índices.
- [ ] `PERF-0125` (`T-46`) Construir el primer diagnóstico de verificación solo en la rama de error.
- [ ] `PERF-0126` (`T-46`) Construir el segundo diagnóstico de verificación solo en la rama de error.
- [ ] `PERF-0127` (`T-46`) Construir el tercer diagnóstico de verificación solo en la rama de error.
- [ ] `PERF-0128` (`T-46`) Construir el cuarto diagnóstico de verificación solo en la rama de error.
- [ ] `PERF-0129` (`T-47`) Añadir `membersDeclaredByType`, índice, bandera y fallback lineal.
- [ ] `PERF-0130` (`T-47`) Usar el índice en `Bodies/TypeKeyPredicates.cpp`.
- [ ] `PERF-0131` (`T-47`) Usar el índice en los cuatro recorridos de `Bodies/CallAnalyzer.cpp`.
- [ ] `PERF-0132` (`T-47`) Usar el índice en `Bodies/ConstructionAnalyzer.cpp`.
- [ ] `PERF-0133` (`T-47`) Sustituir solo el bucle interno de `MethodBodyCallableLifecycle.cpp`.
- [ ] `PERF-0134` (`T-47`) Usar el índice en `HIR/HirBuildCoordinator.cpp`.
- [ ] `PERF-0135` (`T-47`) Usar el índice en `HirCoreCallFrameLowering.cpp`.
- [ ] `PERF-0136` (`T-47`) Usar el índice en `LLVM/Lowering/OopLlvmDebugInfo.cpp`.
- [ ] `PERF-0137` (`T-47`) Usar el índice en `ProjectGenericConstraintModel.cpp`.
- [ ] `PERF-0138` (`T-47`) Añadir y registrar `MembersDeclaredByTypeIndexTests.cpp`.
- [ ] `PERF-0139` (`T-48`) Indexar planes de interfaz por ordinal de clase.
- [ ] `PERF-0140` (`T-48`) Usar ese índice en `Bodies/AssignmentAnalyzer.cpp`.
- [ ] `PERF-0141` (`T-49`) Añadir `CoreTypeOrdinals` a `ProjectMethodBodyModel`.
- [ ] `PERF-0142` (`T-49`) Resolver sus ordinales una vez en `MethodBodyBuildCoordinator`.
- [ ] `PERF-0143` (`T-49`) Copiar la estructura al shard en `MethodBodyCallableLifecycle.cpp`.
- [ ] `PERF-0144` (`T-49`) Sustituir la búsqueda de la raíz de excepciones en `StatementAnalyzer.cpp`.
- [ ] `PERF-0145` (`T-49`) Sustituir la búsqueda de `IDisposable` en `StatementAnalyzer.cpp`.
- [ ] `PERF-0146` (`T-49`) Sustituir la búsqueda de `ThreadStart` en `CallAnalyzer.cpp`.
- [ ] `PERF-0147` (`T-49`) Sustituir la búsqueda de tipo del patrón de string en `CallAnalyzer.cpp`.
- [ ] `PERF-0148` (`T-49`) Sustituir la búsqueda de fachada nativa en `MemberAccessAnalyzer.cpp`.
- [ ] `PERF-0149` (`T-50`) Añadir al name lookup el índice por nombre simple conservando ordinales.
- [ ] `PERF-0150` (`T-50`) Usar el índice en `AssignmentAnalyzer::resolveTypeIdentifier`.
- [ ] `PERF-0151` (`T-50`) Extender la prueba de `T-06` con nombres simples repetidos.
- [ ] `PERF-0152` (`T-51`) Indexar funciones de extensión por nombre en `ProjectNamespaceFunctionModel`.
- [ ] `PERF-0153` (`T-51`) Usar el índice en `MemberAccessAnalyzer::stage38OperatorCandidates`.
- [ ] `PERF-0154` (`T-52`) Indexar plantillas genéricas por `canonicalKey`.
- [ ] `PERF-0155` (`T-52`) Usar el índice en `Bodies/CarrierResolver.cpp`.
- [ ] `PERF-0156` (`T-53`) Reutilizar identidades cacheadas en `bindPrecompiledPhysicalNames`.
- [ ] `PERF-0157` (`T-54`) Crear el índice de identidades de funciones precompiladas.
- [ ] `PERF-0158` (`T-54`) Usarlo en `bindPrecompiledEscapeSummaries` sin tocar `completionScopes_`.
- [ ] `PERF-0159` (`T-54`) Añadir y registrar `PrecompiledNamespaceFunctionIdentityIndexTests.cpp`.

#### Tanda 7 — `0 / 15`

- [ ] `PERF-0160` (`T-55`) Añadir una caché compartida para checksums de fuentes de debug info.
- [ ] `PERF-0161` (`T-55`) Pasar esa caché desde `ProjectOopObjectGenerator` a cada emisor.
- [ ] `PERF-0162` (`T-56`) Indexar bindings de interfaz por el par de tipos consultado.
- [ ] `PERF-0163` (`T-56`) Indexar slots de interfaz por miembro en `MirBuildCoordinator`.
- [ ] `PERF-0164` (`T-57`) Precalcular paquetes, módulos y amigos plegados en `initializeModules`.
- [ ] `PERF-0165` (`T-57`) Usar las formas plegadas en `typeIsVisibleTo` sin cambiar las APIs del IDE.
- [ ] `PERF-0166` (`T-58`) Hacer transparente el hash de `specializationBySpecializedName_`.
- [ ] `PERF-0167` (`T-58`) Indexar por nombre el primer template usado por `closedTypeKeyForTemplate`.
- [ ] `PERF-0168` (`T-59`) Construir el índice `memberOrdinal -> field initializer` en `buildStaticFields`.
- [ ] `PERF-0169` (`T-59`) Usar ese índice en `materializeStaticFieldConstant`.
- [ ] `PERF-0170` (`T-60`) Indexar módulos runtime por nombre de DLL plegado.
- [ ] `PERF-0171` (`T-60`) Deduplicar símbolos con conjuntos locales conservando el orden de salida.
- [ ] `PERF-0172` (`T-61`) Exponer en lectura el índice `functionsByName_` del análisis LLVM existente.
- [ ] `PERF-0173` (`T-61`) Usar el índice en `closureContextDestroy`.
- [ ] `PERF-0174` (`T-61`) Usar el índice en `findCallbackMirFunction`.

**Tracking absoluto:** `302` work items atómicos, `60` paquetes causales y `7` tandas. El avance se actualiza
en `NSC-PENDIENTES-Y-TERMINADOS-RENDIMIENTO.md` únicamente cuando el código del renglón existe en disco y su
evidencia focal está registrada.

---

## 1. Estado certificado (oráculo)

| Elemento | Valor |
|---|---|
| Revisión certificada | Versión 3.0.6 (respaldo de Alberto, 2026-10-20) |
| Unitarias | 3,149 / 3,149 PASS |
| Externas | 6,026 / 6,026 PASS |
| Artefactos a congelar antes del primer commit | `nsc.exe` de 3.0.6. Para hola mundo y el proyecto mediano de `examples/`: dump de MIR, `.ll` por unidad (si se emite), `.obj` por unidad y `.exe` final. Cada uno con su SHA-256, en una carpeta fija junto al respaldo. |

**Regla (LINEAMIENTOS-DE-TRABAJO §8, "Inmutabilidad de la evidencia").** Ninguna prueba, fixture, proyecto ni expectativa se modifica durante esta tarea. Si una prueba falla después de un commit, se presume que la causa es ese commit (§11 del documento de compilador).

---

## 2. Reglas de aplicación para cada entrada

1. **Un commit = una entrada `T-xx`.** Cada entrada es una sola hipótesis causal. No se mezclan entradas ni se agregan "de una vez" limpiezas o mejoras vecinas.
2. **Presupuesto cerrado.** Solo se tocan los archivos y símbolos listados en la entrada. Si hace falta tocar otro, se detiene y se pide autorización (§2, "Presupuesto obligatorio de cambio").
3. **Comentario documental obligatorio en cada función nueva** (§9, "Comentarios obligatorios"): propósito, parámetros, retorno, efectos laterales, casos límite, ownership/lifetime, thread safety, invariantes, algoritmo y complejidad. Los fragmentos de código del documento base **no** traen ese bloque completo; el implementador debe añadirlo.
4. **Ninguna función de más de 60 líneas ejecutables, ninguna recursión nueva, ninguna lambda nueva.**
5. **No se borra ningún comentario.** Si se sustituye el código que un comentario describe, el comentario se conserva y se añade debajo `// PERF (T-xx): ...`.
6. **Pruebas nuevas en archivos nuevos** (§5.1 del documento de compilador). Cada índice nuevo trae una prueba diferencial contra el recorrido lineal que sustituye.
7. **Gate antes de entregar cada entrada** (§2, "Gate de diferencial"): para cada archivo y símbolo tocado se responden las 8 preguntas del lineamiento y se anotan en el commit.
8. **Validación.** Alberto ejecuta la suite completa desde Builder. El implementador entrega la entrada; no declara PASS sin esa corrida.

---

## 3. Requisito transversal descubierto al leer las pruebas (HECHO, obligatorio)

`tests/unit/ProjectOopLoweringTests.cpp` construye modelos **a mano**, sin llamar a `build()`. Rellena miembros privados directamente:

- `nameLookup.namedTypes_` (líneas 62, 82, 194, 609, 628, 693, 710, 734 y 838);
- `objectLayoutModel.structLayouts_`, `structLayoutIndexByType_` y `classLayouts_` (97-125, 165-176, 648-665);
- `memberModel.members_` y `membersByType_` (599-601, 672-682, 847, 1182, 1621, 1669);
- y fija `built_ = true` en `ProjectNameLookup`, `ProjectObjectLayoutModel`, `ProjectMemberModel`, `ProjectConstructionModel` e `ProjectInheritanceModel`.

Además vacía (`clear()`) y vuelve a llenar `namedTypes_` (609 y 693) en distintos fixtures.

**Consecuencia para todos los índices nuevos (T-06, T-15, T-16, T-47 y siguientes):**

- El índice se construye **solo** dentro de `build()` y lleva una bandera propia (`...IndexBuilt_`), que **no** es `built_`.
- Si la bandera es `false`, la consulta ejecuta **el recorrido lineal original**. Esto no es opcional: sin el fallback, esas pruebas consultarían un índice vacío y fallarían.
- Prohibido construir el índice "perezosamente" en la primera consulta. Un fixture que hace `clear()` y vuelve a llenar los vectores dejaría el índice obsoleto.

---

## 4. Verificaciones cerradas hoy (antes NO VERIFICADO)

| Pregunta | Resultado | Evidencia |
|---|---|---|
| ¿`scanParallelOrdinals` y `applyParallelOrdinalOffsets` aceptan una función sin cuerpo? (R-03.1) | **Sí** | `HirBuildCoordinator.cpp:266-272` y `:300-306`: pila con `if (current == nullptr) continue;` |
| ¿Hay `ObjectFieldLayout::memberOrdinal` fuera del rango de `members()`? (R-04.2) | **No** | `ProjectObjectLayoutModel.cpp:867-870` y `:1168-1171` usan `memberModel_->members()[memberOrdinal]`. Los promovidos (`:1296`) usan `aliasOrdinal`, comprobado contra `members().size()` en `:1250`. El índice de tamaño `members().size()` es exacto. |
| ¿Las pruebas nombran `knownTypeNames_`, `functionByMember_`, `Cow`, `originNamespace()`, `declaredTypeIdentities_` o las trazas? | **No** | grep sobre los 723 `.cpp`/`.h` de `tests/unit` (copiados hoy completos): 0 coincidencias para cada uno |
| ¿Alguna prueba liga el resultado de `key()` o `fullyQualifiedName()` a `&&` o a un temporal? (R-01) | **No** | grep: 0 coincidencias. 7 archivos usan `"type:" + x.identity.key()`, que es compatible con `const std::string&`. |
| ¿Alguna prueba toca `structLayouts_` de MIR? | **No** | Solo el de `ProjectObjectLayoutModel`, en `ProjectOopLoweringTests.cpp`; ver §3 |
| ¿Hay pruebas que usen `fieldLayoutForMember`? | **Sí, sobre modelos compilados** | `Stage41ArraysStringsCoreTests.cpp:227-230`, `ExplicitFieldAlignmentTests.cpp:47`, `FixedMultidimensionalArrayTests.cpp:86-90` |
| ¿Hay pruebas de SHA-256? | **Sí** | `Stage64InstallationManifestTests.cpp:84-104` (vectores vacío y "abc", más `sha256File`) y `ProjectCallableWaveDriverTests.cpp:68` |
| ¿`nsc build` reenvía `--time`? | **No** | `ProjectCommands.cpp:1280-1301` solo reenvía `--library-reference-root`, `-j` y `--progress[...]`. `DriverPhaseTimer.cpp:9` también se activa con `reportProgress`, así que para medir se usa `nsc build --progress`. |
| ¿NSLink enlaza una DLL dummy para generar cada import library? (R-08.2) | **No.** El costo real es un proceso por DLL, no un enlace. | `A:\nslink-devel\src\driver\CompatibleDriver.cpp:782-795`: con `/def` y sin objetos de entrada, el producto es `ImportLibrary` **antes** de mirar `/dll`. `:900-923`: escribe directo con `ImportLibraryWriter::write`. `nsc` pasa `/dll /noentry /def /implib /out:dummy.dll` sin objetos (`LldLinker.cpp:604-610`), así que ya cae en esa ruta. |
| ¿Es seguro no clonar los tipos hoja en `prepareNodeScope`? (R-06.4) | **No demostrado** | Los consumidores clonan la instantánea (`GenericTypeInference.cpp:122-123`, `cloneInferredType`) y no comparan punteros. Pero hay mutadores en sitio de `TypeSyntax`: `setBaseName` en `TypeAliasExpander.cpp:196` y `ConstGenericArguments.cpp:177`/`:194`, y `setOrigin*`. No se puede probar que un tipo hoja no cambie después de la instantánea. Queda NO IMPLEMENTAR. |

---

## 5. Clasificación de autorización

### Grupo A — optimización interna con equivalencia estricta

Dentro del alcance de una tarea de rendimiento autorizada (§12 del documento de compilador). Las entradas T-01 a T-62 de la §6, salvo las marcadas B.

### Grupo B — requiere autorización explícita y separada

Cada una cambia algo protegido por §12 o por §8 ("formatos persistentes, artefactos emitidos, comportamiento de compilación o ejecución"), o retira una verificación existente.

| ID | Qué cambia | Por qué no es A |
|---|---|---|
| B-01 (R-09.3) | Deja de emitir la traza de runtime con `debug=full` (`OopLlvmRuntimeDebugTrace.cpp:13`) | Cambia el **programa generado**: el `.exe` deja de escribir `A:\temp\nsc-runtime-debug-trace.log`, y cambian el IR y los artefactos. §12: "comportamiento observable del programa generado". |
| B-02 (R-09.1) | Apaga `ColdLlvmTrace` (`ColdLlvmTrace.cpp:22-23`) | Retira instrumentación. §9.2: ninguna eliminación se autoriza por anticipación. |
| B-03 (R-09.2) | Apaga `tracePhase` (`OopLlvmImplementation.cpp:166-189`) | Igual que B-02. El comentario del propio código dice "Remove only after the remaining coordinator cost has a measured owner". |
| B-04 (R-08.1) | Caché de `runtime.obj` | Formato persistente nuevo en la caché de artefactos |
| B-05 (R-08.2) | Caché de import libraries | Formato persistente nuevo. Sin él, el ahorro máximo es de 4 o más procesos por build. |
| B-06 (R-10.1) | Caché del texto de la superficie pública de Core | Formato persistente nuevo |
| B-07 (R-15.3) | Podar la caché con menos frecuencia | Cambia la política observable de la caché. Revisar `CompilationArtifactCacheTests.cpp`. |
| B-08 (R-07.3.2) | No re-hashear el destino del restore | Retira la detección de corrupción del destino |
| B-09 (R-07.3.3) | No reescribir el lock si es igual | Cambia el mtime del lock, que es observable para herramientas externas |
| B-10 (R-07.5.2) | Reutilizar el NSMETA ya verificado en `validateLibraryReference` | La segunda lectura hoy detecta un `.nsmeta` reemplazado entre configuración y enlace. Reutilizar retira esa garantía. |
| B-11 (R-08.3.2) | Memoizar la verificación PE de las DLL de runtime | Igual que B-10 |

### Grupo C — NO IMPLEMENTAR (hallazgo no autorizado, §2)

Hace falta una evidencia o un diseño que no existe todavía.

| ID | Hallazgo | Estado |
|---|---|---|
| C-01 (R-06.4) | No clonar los tipos hoja | Equivalencia no demostrada (ver §4) |
| C-02 (R-06.5) | Banderas por unidad y fusión de pasadas de genéricos | Hay que demostrar la independencia del orden entre pasadas |
| C-03 (R-11.5) | Partir la superficie de Core en varias unidades | Cambia los ordinales de unidad y el texto generado |
| C-04 (R-12) | Arena real y nodo AST compacto | Estructural; afecta pruebas con acceso privado |
| C-05 (R-08.5) | No destruir `ProjectCompilation` en la CLI | Cambia el comportamiento del proceso |
| C-06 (R-07.4.5) | Pasar el grafo ya cargado al driver anidado | Cambia una interfaz entre drivers |
| C-07 (R-13.7) | Mapas perezosos en los shards de cuerpos | Toca miembros usados por pruebas |
| C-08 (R-15.4) | Reutilizar un lowerer MIR por hilo | Diseño |
| C-09 (R-06.8 parte 2) | Quitar la recursión de `std::function` en `buildVisibility` | Es limpieza de estilo, no rendimiento. §2 prohíbe mezclarla. Solo entra la precomputación de la ruta canónica (T-36). |
| C-10 (R-11.3 alternativas) | Hash transparente en `knownTypeNames_` o conjunto base compartido | Cambia el tipo o el contenido de un miembro. Solo entra la versión con buffer reutilizado (T-39). |
| C-11 | Recursión existente en `callTypeIsSend`/`callTypeIsSync` | Preexistente. Fuera del alcance de rendimiento. Se registra y no se toca. |
| C-12 | Copias de strings y vectores HIR→MIR por nodo (`MirNodeMachine::copyIdentity`, `copyControlState`, `copyAttributes`; `copyFunctionMetadata`) | Inherentes al diseño actual: el MIR posee sus strings. Reducirlas exige internar los `typeKey` (diseño). |

---

## 6. Tabla de tracking — Grupo A

Columnas:

- **Gate:**
  - `=ART` = MIR, `.obj` y `.exe` byte-idénticos al oráculo en hola mundo y en el proyecto mediano, más la suite completa en PASS sin tocar pruebas;
  - `=PROC` = mismos artefactos y menos trabajo, verificado con un contador.
- **Prueba nueva:** archivo nuevo en `tests/unit/` y su registro en `CMakeLists.txt` siguiendo el patrón de `nsc_precompiled_member_identity_index_tests` (`CMakeLists.txt:5878-5886`).
- **Depende de:** entradas que deben estar integradas antes.
- **Estado inicial de todas:** PENDIENTE.

### Lote 1 — sin índices nuevos

| T | Origen | Presupuesto (archivos → símbolos) | Gate | Prueba nueva | Depende de |
|---|---|---|---|---|---|
| T-01 | R-01 | `include/nativesharp/Compilation/TypeIdentity.h` → `fullyQualifiedName`, `key`, nuevos `fullyQualifiedName_`, `key_`, `buildFullyQualifiedName`. `src/Compilation/TypeIdentity.cpp` → ctor (76-89), 126-158. | =ART | `TypeIdentityCacheTests.cpp`: para identidades con namespace, tipos anidados (`+`), aridad y combinaciones, comparar `key()`/`fullyQualifiedName()` contra la concatenación reconstruida en la prueba con la regla de `TypeIdentity.cpp:126-158`. Verificar también que una copia de la identidad conserva los mismos valores. | — |
| T-02 | R-07.1 (`sha256`) | `src/Basic/Sha256.cpp` → `sha256` (112-121), nuevo `sha256BlockWords`. `sha256Words` queda delegando. `padSha256Input` se conserva. | =ART | `Sha256StreamingEquivalenceTests.cpp`: la prueba lleva su propia copia literal del algoritmo antiguo como referencia. Compara longitudes 0..300, 1 MiB pseudoaleatorio con semilla fija y los vectores FIPS "", "abc" y la de 448 bits. | — |
| T-03 | R-07.1 (`sha256File`) | `src/Basic/Sha256.cpp` → `sha256File` (131-136) | =ART | Misma prueba que T-02: archivo temporal de 0, 1, 65535, 65536, 65537 y 200000 bytes; `sha256File` debe coincidir con `sha256` sobre el contenido. | T-02 |
| T-04 | R-08.3 (lectura) | `src/Linker/PeExecutableVerifier.cpp` → `readPeBytes` (121-122) | =ART | No hace falta: lo cubren las pruebas PE existentes (`Stage45LinkerPeEhTests.cpp`, `WindowsLinkerTests.cpp`) | — |
| T-05 | R-05.1 | `src/LLVM/Lowering/OopLlvmTypesAbiAttributes.cpp` → `llvmType` (11-19) | =ART | No hace falta | — |
| T-25 | R-07.2 | `src/Driver/ProjectCommands.cpp` → `executeBuild` (1455-1456) | =ART, y la salida de `nsc build --dependency-report` idéntica | No hace falta: lo cubren `ProjectCommandsTests.cpp` y `DriverTests.cpp` | — |
| T-26 | R-07.3.1 y R-07.3.4 | `src/Driver/ProjectCommands.cpp` → `appendArtifact` (canónico de la raíz por paquete; firma interna nueva) y `executeRestore:649` (`std::move(plan)`) | =ART | No hace falta | — |
| T-27 | R-07.5.1 | `src/Compilation/PrecompiledProjectPreparation.cpp` → `loadExplicitMetadata`. Caché local `foldedPathKey(metadataPath) → moduleName`. Cuando la ruta se repite, se reproduce la decisión sin leer: si el módulo está en `sourceProjects`, `sourceReplacements.push_back(...)` como en la ruta vigente; si no, `continue`, porque la ruta y la huella son las mismas que la entrada ya cargada. | =ART, con los mismos diagnósticos cuando hay conflicto | `PrecompiledMetadataDedupTests.cpp`: dos proyectos que referencian el mismo `.nsmeta`; uno que referencia dos rutas distintas del mismo módulo (debe seguir dando el mismo error, sin cambios en el texto) | — |

### Lote 2 — índices

| T | Origen | Presupuesto | Gate | Prueba nueva | Depende de |
|---|---|---|---|---|---|
| T-06 | R-02 (infra) | Nuevo `include/nativesharp/Frontend/TransparentStringHash.h`. `ProjectNameLookup.h` → `typeOrdinalForIdentityKey`, `typeOrdinalForTypeKey`, `indexTypeOrdinalsByIdentityKey`, `typeOrdinalByIdentityKey_`, `typeOrdinalByIdentityKeyBuilt_`. Nuevo `src/Frontend/NameLookup/TypeIdentityKeyIndex.cpp`. `ProjectNameLookup.cpp:94` (una línea). `CMakeLists.txt` (una fuente). | =ART | `NameLookupIdentityKeyIndexTests.cpp`: compilar un proyecto con `A.B` (namespace) y `A+B` (anidado) más tipos de Core. Para **todo** `namedTypes()`, el índice debe coincidir con el recorrido lineal. Además, un modelo armado a mano sin `build()` debe usar el fallback (requisito de la §3). | — |
| T-07 | R-02 #1-#3 | `ProjectInheritanceModel.cpp:264-280`; `ProjectInterfaceModel.cpp:327-343`; `ProjectGenericConstraintModel.cpp:63-78` | =ART | No hace falta (cubierto por T-06) | T-06 |
| T-08 | R-02 #4-#7 | `ProjectObjectLayoutModel.cpp` → `valueLayoutForTypeKey` (222-225), `hasTrivialStructLifetime` (615-618), `structBitFieldWidth` (653-655), `isClassBitFieldBase` (1065-1067) | =ART | No hace falta | T-06 |
| T-09 | R-02 #8-#13 | `HIR/HirBuildCoordinator.cpp` (2107-2166); `HIR/HirExpressionMachine.cpp` (476-484; se conserva `isEnumTypeWithIdentity`); `HIR/HirCallLowering.cpp` (1646-1652); `HIR/ExpressionStatementMachine/HirCoreCallFrameLowering.cpp` (589-595) | =ART | No hace falta | T-06 |
| T-10 | R-02 #14 | `MIR/Internal/MirNodeMachine.cpp` (1976-1983) | =ART | No hace falta | T-06 |
| T-11 | R-02 #15-#28 | `Bodies/MethodBodyTypeQueries.cpp` (48, 65); `Bodies/TypeKeyPredicates.cpp` (61, 97, 133); `Bodies/ConstructionAnalyzer.cpp` (45, 57, 77, 299, 329, 376); `Bodies/LambdaDelegateBinder.cpp` (64); `Bodies/ConversionAndPassKindChecks.cpp` (147, 287) | =ART | No hace falta | T-06 |
| T-12 | R-02 #36 | `MemberModel/BuildAndDeclaration.cpp` (97-108) | =ART | No hace falta | T-06 |
| T-13 | R-02 #40-#50 | `LLVM/LlvmTypeLayout.cpp` (25-38); `LLVM/LlvmTypeClassification.cpp` (13-41); `LLVM/Lowering/OopLlvmDebugInfo.cpp` (341-350); `OopLlvmThunksAndWrappers.cpp` (571-602); `OopLlvmTypesAbiAttributes.cpp` (262-342, incluido `const std::string name` → `const std::string&`) | =ART | No hace falta | T-06 |
| T-14 | R-02 #51-#52 | `Interop/CHeaderGenerator.cpp` (130-141, 289-291) | =ART, con el header C generado idéntico | No hace falta | T-06 |
| T-15 | R-04.2 | `ProjectObjectLayoutModel.h` → `fieldLayoutByMember_`, `fieldLayoutByMemberBuilt_`, `indexFieldLayoutsByMember`. `ProjectObjectLayoutModel.cpp` → `build` (antes de 122) y `fieldLayoutForMember` (161) | =ART | `FieldLayoutIndexTests.cpp`: compilar un proyecto con structs, clases, uniones anónimas y promovidos. Para cada miembro, el índice debe coincidir con el recorrido lineal antiguo, reproducido en la prueba sobre `structLayouts()`/`classLayouts()`. | — |
| T-16 | R-04.3 | `ProjectOopMirModel.h` → `structLayoutForTypeKey`, `classLayoutForTypeKey`, dos mapas y una bandera. `MirBuildCoordinator.cpp` → `build` (antes de 657). `LLVM/LlvmTypeLayout.cpp` → `findStructLayout`/`findClassLayout` delegan, con fallback. | =ART | `MirLayoutIndexTests.cpp`: para cada `typeKey` de `structLayouts()` y `classLayouts()`, el índice debe coincidir con el primero encontrado linealmente | — |
| T-17 | R-04.4 | `MIR/MirFunctionLowerer.cpp` → `abiPassingForType` (256-262), `configureFunctionAbi` (264-282), `sumAwaitedFrameSizeNodes` (290-310): sobrecargas con índice; se conservan las de `span`. `lowerOneFunction` y `lowerFunctions` pasan el índice. | =ART | No hace falta | T-16 |
| T-18 | R-04.1 | `ProjectOopMirModel.h` → `sharedStructLayouts_`, `activeStructLayouts`. `MirFunctionLowerer.cpp:844`; `MirNodeMachine.cpp:1593` y `:1626` | =ART | No hace falta | — |
| T-19 | R-05.2 | `LLVM/Internal/OopLlvmImplementation.h` → `llvmTypeCache_`. `OopLlvmTypesAbiAttributes.cpp` → `llvmType` | =ART | No hace falta | T-05 |

### Lote 3 — HIR

| T | Origen | Presupuesto | Gate | Prueba nueva | Depende de |
|---|---|---|---|---|---|
| T-20 | R-03.1 (coordinador) | `HirBuildCoordinator.cpp` → nuevos `makePrecompiledRegularFunction` y `makeImportedCRegularFunction` (cuerpos movidos tal cual de 889-920 y 934-962, comentarios incluidos), `resolveRegularFunctionWithoutBody`; `buildRegularFunctionsInParallel` (743-752). `ProjectOopHirModel.h` (3 declaraciones). | =ART, con el mismo orden de `functions()` (dump HIR idéntico) | `HirParallelOrderTests.cpp`: compilar con `-j1` y con `-jN`, y comparar el dump HIR completo | — |
| T-21 | R-03.1 (worker) | `HirBuildCoordinator.cpp` → `buildImpl` (2068-2070), nuevo `recordFunctionForMember`; escrituras en 921, 963 y 1479 | =ART | La misma de T-20 | T-20 |
| T-22 | R-03.1 (filtro) | `HirBuildCoordinator.cpp` → `buildRegularFunctionsSequentially` (1538-1548) | =ART | La misma de T-20 | — |
| T-23 | R-03.2 | `ProjectOopHirModel.h` → 3 índices y 3 accesores. `HirBuildCoordinator.cpp` → `indexBuildBindings` (706-734), sitios 1015-1020, 1029-1036, 1590-1600, 1666-1671 y 1796-1802. `HirBindingIndex.cpp` (accesores). | =ART | La misma de T-20, más un caso con lambdas anidadas y parámetros `owned` | — |
| T-24 | R-03.3 | `HirBuildCoordinator.cpp` → `markLambdaClosureCells` (1827-1880) | =ART | No hace falta | — |

### Lote 4 — driver y carga

| T | Origen | Presupuesto | Gate | Prueba nueva | Depende de |
|---|---|---|---|---|---|
| T-29 | R-07.4.1-4 | `Compilation/ProjectSourceSet.cpp` → `add` (14-72), `finalize` (74-…); `Compilation/ProjectGraphLoader.cpp` → `normalizeTransitiveSourceOwnership` (140-185), `preferExactSourceModuleArtifacts` (188-…) | =ART, con el mismo grafo y el mismo orden de fuentes | No hace falta: lo cubren `ProjectLoaderTests.cpp`, `ProjectGraphTests.cpp` y `ProjectLoadSessionTests.cpp` | — |

### Lote 5 — genéricos, lexer y parser

| T | Origen | Presupuesto | Gate | Prueba nueva | Depende de |
|---|---|---|---|---|---|
| T-30 | R-06.1 | `ProjectGenericModel.h` (1576, nuevo índice); `Generics/TemplateCollector.cpp:165-169`; `Generics/TypeSpellingAndVisibility.cpp` (219-243, 293-318) | =ART, con el dump de genéricos idéntico | `DeclaredTypeIdentityIndexTests.cpp`: tipos con el mismo `sourceName` en dos proyectos, públicos y privados | T-06 (por `TransparentStringHash.h`) |
| T-31 | R-06.2 | `ProjectGenericModel.h` (384-395, `Cow`) | =ART | No hace falta | — |
| T-62 | R-06.2 extra | `Generics/GenericNodeProcessor.cpp` (107-110) | =ART | No hace falta | — |
| T-33 | R-06.3 | Hasta 6 sitios: `ProjectGenericModel.cpp:247`, `TypeAliasExpander.cpp:384`, `GenericNodeProcessor.cpp:447`, `TemplateDependencyCollector.cpp:181` y `:370`, `TemplateCollector.cpp:284`. **Solo** donde se demuestre que el cuerpo no muta `children()` del nodo; cada sitio descartado se anota con su razón. | =ART | No hace falta | — |
| T-34 | R-06.6 | `include/nativesharp/AST/TypeSyntax.h:232` y su `.cpp` | =ART | No hace falta (0 usos en las pruebas) | — |
| T-35 | R-06.7 | `Generics/TemplateResolver.cpp` (133-152); `Generics/TypeDemandScheduler.cpp` o el archivo de `processTypeSyntax` (573-579) | =ART | No hace falta | T-06 |
| T-36 | R-06.8 (parte 1) | `ProjectGenericModel.cpp` → `buildVisibility` (398-428): **solo** precomputar `pathKey(weakly_canonical(...))` por proyecto. La recursión queda como está (C-09). | =ART | No hace falta | — |
| T-37 | R-11.1 | `Lexer/Lexer.cpp` (55-201) | =ART | No hace falta: lo cubre `LexerTests.cpp` | — |
| T-38 | R-11.2 | `Lexer/Lexer.cpp` → `lexAll` (219-235) | =ART | No hace falta | — |
| T-39 | R-11.3 (variante con buffer) | `include/nativesharp/Parser/Parser.h` (miembro `mutable std::string typeNameLookupScratch_`); `Parser/ParserContext.cpp` (49, `reserve`); `Parser/TypeParser.cpp:57` | =ART | No hace falta | — |
| T-40 | R-11.4 | `Frontend/ProjectCompilation.cpp` → `collectParserKnownTypeNames` (484-500) | =ART, con la misma lista en el mismo orden | No hace falta | — |

### Lote 6 — superficie, metadatos, cuerpos y enlace con precompilados

| T | Origen | Presupuesto | Gate | Prueba nueva | Depende de |
|---|---|---|---|---|---|
| T-41 | R-10.2 | `Driver/PublicSurfaceBuilder.cpp` → `collectPublishedMembers` (1298-1299), `publishContractTypes` (1347-1348), `orderedPublishedMethods` (1149-1151); nuevo `findMetadataTypeByTypeKey`. Ruta `lower_bound` solo si `payloadDecodedByCurrentReader`. | =ART, con el texto `public-api` byte-idéntico | `PublicSurfaceLookupTests.cpp`: el texto emitido debe ser idéntico con metadata decodificada y con metadata construida en memoria (sin ordenar) | — |
| T-42 | R-10.3 | `Driver/PublicSurfaceBuilder.cpp` → `orderedPublishedMethods` (1153-1160), `dispatchRank` (1204-1234) | =ART, con el texto byte-idéntico | La misma de T-41 | T-41 |
| T-43 | R-10.4 | Nuevo `Driver/PrecompiledSurface/ContractMaterializationCache.h`/`.cpp`; sobrecargas en `ContractMaterialization.cpp` (31-36, 154-158, 180-231, 296-301, 499-561) | =ART | `ContractMaterializationCacheTests.cpp`: con y sin caché, mismo resultado para cada miembro de la metadata de Core | — |
| T-44 | R-10.5 | `Interop/LibraryMetadataVerification.cpp` → `verifyGenericTemplateClosure`/`verifyGenericTemplates` (839-907) | =ART, con los mismos diagnósticos | No hace falta | — |
| T-45 | R-10.6 | `Interop/LibraryMetadataVerification.cpp` → `verifyMetadataDispatchContracts` (675-702), `verifyVirtualMemberDispatch` (644-645), `findMetadataDispatch*` (595-618) | =ART | No hace falta | — |
| T-46 | R-10.7 | `Interop/LibraryMetadataVerification.cpp` (1492-1505, 1530, 1560, 1574-1584) | =ART, con el **texto de cada diagnóstico idéntico** | No hace falta | — |
| T-47 | R-13 (infra y sitios) | `ProjectMemberModel.h`/`.cpp` → `membersDeclaredByType`, índice y bandera, construidos al final de `build`, conservando sin cambios el acceso público vigente `typeOrdinalForTypeKey`. Sitios: `TypeKeyPredicates.cpp:113`; `CallAnalyzer.cpp:2232`, `:2444`, `:4921`, `:4942`; `ConstructionAnalyzer.cpp:232`; `MethodBodyCallableLifecycle.cpp:384` (solo el bucle interno); `HirBuildCoordinator.cpp:1364`; `HirCoreCallFrameLowering.cpp:604`; `OopLlvmDebugInfo.cpp:865`; `ProjectGenericConstraintModel.cpp:867` | =ART, con los diagnósticos en el mismo orden | `MembersDeclaredByTypeIndexTests.cpp`: el índice contra el filtro lineal para todos los tipos, incluidos destructores y miembros promovidos; y el fallback en un modelo armado a mano | — |
| T-48 | R-13.1 | `ProjectInterfaceModel.h`/`.cpp` (índice `classTypeOrdinal → plans`); `AssignmentAnalyzer.cpp:2566-2585` | =ART | No hace falta | — |
| T-49 | R-13.4/13.5 | `ProjectMethodBodyModel.h` (`CoreTypeOrdinals`); `MethodBodyBuildCoordinator.cpp` (cálculo); `MethodBodyCallableLifecycle.cpp:250-270` (copia al shard); `StatementAnalyzer.cpp:2106-2122` y `:2182-2198`; `CallAnalyzer.cpp:1855`, `:3073`; `MemberAccessAnalyzer.cpp:412` | =ART | No hace falta | T-01 |
| T-50 | R-13.6 | `ProjectNameLookup` (índice por nombre simple, junto al de T-06); `AssignmentAnalyzer.cpp:2634-2660` | =ART | Extender la prueba de T-06 con nombres repetidos | T-06 |
| T-51 | R-13.8 | `ProjectNamespaceFunctionModel.h`/`.cpp` (índice de extensiones por nombre, independiente de `completionScopes_`); `MemberAccessAnalyzer.cpp:1258-1268` | =ART | No hace falta | — |
| T-52 | R-13.9 | `ProjectGenericModel` (índice `canonicalKey → template`); `CarrierResolver.cpp:153-224` | =ART | No hace falta | T-47 |
| T-53 | R-14.1 | `MemberModel/BuildSupport.cpp` → `bindPrecompiledPhysicalNames` (316-390) | =ART | No hace falta | T-01 |
| T-54 | R-14.2 | Nuevo `PrecompiledNamespaceFunctionIdentityIndex.h`/`.cpp`; `ProjectNamespaceFunctionModel.cpp` → `bindPrecompiledEscapeSummaries`, sin sustituir ni alterar `completionScopes_` | =ART, con los mismos mensajes de error | `PrecompiledNamespaceFunctionIdentityIndexTests.cpp`, siguiendo el patrón de `PrecompiledMemberIdentityIndexTests.cpp` | — |

### Lote 7 — varios (P2)

| T | Origen | Presupuesto | Gate | Prueba nueva | Depende de |
|---|---|---|---|---|---|
| T-55 | R-15.1 | `LLVM/Lowering/Support/DebugInfoEmission.cpp` (111-149) y el paso de la caché compartida desde `ProjectOopObjectGenerator.cpp` | =ART con `debug=full` | No hace falta | T-02 |
| T-56 | R-15.2 | `MIR/MirBuildCoordinator.cpp` → `interfaceBindingForTypes`, `interfaceSlotForMember` (~760-785) | =ART | No hace falta | — |
| T-57 | R-15.5 | `NameLookup/TypeVisibility.cpp`; `NameLookup/ProjectSetup.cpp` (`initializeModules`). Precalcular formas plegadas conservando las rutas públicas vigentes `typeIsVisibleFromProject` y `nestedTypeIsVisibleFrom`. | =ART, con los mismos diagnósticos de visibilidad | No hace falta | — |
| T-58 | R-15.7 | `ProjectGenericModel.cpp` (630-660) | =ART | No hace falta | T-06 |
| T-59 | R-15.8 | `MIR/MirBuildCoordinator.cpp` → `materializeStaticFieldConstant` (439-…), `buildStaticFields` (548) | =ART | No hace falta | — |
| T-60 | R-15.9 | `Driver/LibraryReferenceValidation.cpp:9-21` y sus 4 llamadores | =ART | No hace falta | — |
| T-61 | R-15.10 | `LLVM/ProjectOopFunctionAnalysis.h` (exponer `functionsByName_`); `OopLlvmClosuresDelegatesEvents.cpp:251-258`; `OopLlvmThunksAndWrappers.cpp:144-…` | =ART | No hace falta | — |

**Numeración.** Los huecos (T-28, T-32) corresponden a entradas que pasaron al Grupo B (B-10) o se fusionaron en T-62. No se reutilizan, para no confundir referencias cruzadas.

---

## 7. Registro por entrada (llenar al cerrar cada T)

```text
T-xx  Origen: R-..   Commit: <hash>   Fecha:
Presupuesto respetado: SI/NO (si NO: detalle y autorización)
Gate de diferencial (8 preguntas, §2): ...
Comentarios conservados: SI   Comentario PERF añadido: SI
Funciones nuevas con comentario documental completo: SI
Suite unitaria (Builder): x/3149   Externa: y/6026
Artefactos vs oráculo: MIR = / .obj = / .exe =   (o diff esperado documentado)
Medición (si aplica, host /O2): antes ... después ...
Estado: CERRADO / REABIERTO (causa)
```

---

## 8. Orden recomendado y criterio de parada

1. **Lote 1**: T-01 a T-05, T-25 a T-27. Cambios locales sin índices.
2. **Lote 2**: T-06 primero (infraestructura y su prueba), luego T-07 a T-19.
3. **Lote 3**: T-20 a T-24.
4. **Lotes 4 a 7**.
5. **Grupo B**: solo con la autorización de Alberto, entrada por entrada.

**Criterio de parada.** Si una entrada rompe la suite, se detiene el lote. Se revierte **solo** esa entrada, se analiza contra el oráculo (LINEAMIENTOS-DE-TRABAJO §8, "Revisión obligatoria contra el último estado correcto") y no se corrigen consumidores ni pruebas.


---

## 9. Protocolo de ejecución de esta campaña

```text
Implementa el tracking NSC-IMPLEMENTACION-RENDIMIENTO.md (ediciones exactas en NSC-ANALISIS-RENDIMIENTO-POR-FUNCION.md,
leer primero su §0.5 de correcciones). Antes de tocar nada lee completos A:\LINEAMIENTOS-DESARROLLO-COMPILADOR-NATIVESHARP.md
y las secciones §2, §8, §9 y §10 de A:\LINEAMIENTOS-DE-TRABAJO-Y-PORTEO.md.
- Toda la implementacion corresponde a Codex en A:\nsc-devel. El IDE y su agente quedan fuera de alcance y no reciben tareas.
- Solo Grupo A (§6), en el orden de §8. Grupo B solo con autorizacion explicita de Alberto por entrada. Grupo C: no tocar.
- Una entrada T-xx = un commit = una hipotesis. Presupuesto cerrado: solo los archivos y simbolos listados.
- Equivalencia estricta contra el oraculo 3.0.6 (3149/3149 unitarias, 6026/6026 externas). No modificar pruebas, fixtures
  ni proyectos. Cada indice nuevo: construido solo en build(), con bandera propia y fallback al recorrido lineal original
  (ProjectOopLoweringTests arma modelos a mano sin build()).
- Funciones nuevas con comentario documental completo; <=60 lineas ejecutables; sin recursion ni lambdas nuevas; no borrar
  comentarios (anadir "// PERF (T-xx): ..." debajo).
- Pruebas nuevas en archivos nuevos donde la tabla lo indica, registradas como nsc_precompiled_member_identity_index_tests.
- Al terminar cada entrada: llenar el registro de §7 y entregarla para que Alberto corra la suite desde Builder. No declarar
  PASS sin esa corrida.
```
