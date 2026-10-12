# Test plan — Host.Language

## Estado

- **Clasificación:** `DISEÑO PROPUESTO`.
- **Implementación/ejecución:** ninguna.
- **Unidad lógica:** casos parametrizados por contrato estable; nunca por cantidad manual de `if`.
- **Oráculos:** retorno, error exacto, estado antes/después, orden, bytes, fingerprint y side effects.
- **Cobertura instrumental:** expresamente diferida a Fase 3.

Cada fila puede expandirse a múltiples vectores dentro del mismo test parametrizado. La matriz descrita
forma parte del caso: no se permite escoger sólo una muestra positiva.

## ABI, FFI y SIMD — `CL-LANG-001`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LANG-001 | CL-LANG-001 | representation layouts validos: align/pack 1,2,4,8,16 y transparent de un campo | acepta exactamente las combinaciones permitidas; resultado estable | PR |
| TL-LANG-002 | CL-LANG-001 | representation layouts invalidos: cero, no potencia, mayor de 16, transparent con 0 o varios campos | falla con el diagnostico exacto y sin normalizar la entrada | PR |
| TL-LANG-003 | CL-LANG-001 | bitFieldMask para storage 8/16/32/64, bordes offset/width y width 64 | mascara exacta o nullopt cuando el rango no cabe | PR |
| TL-LANG-004 | CL-LANG-001 | pointerContractImplicitlyAssignable sobre matriz nullability/mutability/provenance/alignment/address-space | booleano exacto para cada celda de la matriz | PR |
| TL-LANG-005 | CL-LANG-001 | pointerCastRequiresUnsafe sobre la misma matriz | es el complemento contractual de assignability en los cambios restringidos | PR |
| TL-LANG-006 | CL-LANG-001 | callingConventionSupportedOnWindowsX64 para todas las convenciones con variadic false/true | solo Cdecl y Win64 permiten variadic; matriz completa exacta | PR |
| TL-LANG-007 | CL-LANG-001 | unsafeOperationRequiresContext para todos los UnsafeOperation y un valor externo | true para las cinco operaciones; fallback vigente caracterizado | PR |
| TL-LANG-008 | CL-LANG-001 | unsafeBlockTooBroad con 0,8,9,max statements y razon ausente/presente | umbral estricto y excepcion por razon exactos | PR |
| TL-LANG-009 | CL-LANG-001 | validateFfiParameter sobre tipos validos, vacio, indices extremos y conteo | aceptacion o primer error exacto sin mutar parametros | PR |
| TL-LANG-010 | CL-LANG-001 | ffiMayUnwindAcrossBoundary repetido | false estable y sin estado global | PR |
| TL-LANG-011 | CL-LANG-001 | validateSimd sobre lane widths, counts potencia/no potencia y total 512/513 | aceptacion y diagnostico exactos | PR |
| TL-LANG-012 | CL-LANG-001 | portableSimdType e identity round-trip para simd/mask y tipos de lane representativos | valor, identidad y parse reconstruyen todos los campos | PR |
| TL-LANG-013 | CL-LANG-001 | parsePortableSimdType con prefijo, separadores, count, trailing y overflow invalidos | nullopt sin aceptar prefijos parciales | PR |
| TL-LANG-014 | CL-LANG-001 | validateSimdShuffle y simdMaskLane con indices completos, faltantes, fuera de rango y lane bits | orden exacto; mascara cero o todos los bits uno; fallback escalar true | PR |

## Agregados — `CL-LANG-002`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LANG-015 | CL-LANG-002 | tupleIdentity para 0,1,N elementos con nombres distintos | bytes length-prefixed exactos; nombres no aparecen | PR |
| TL-LANG-016 | CL-LANG-002 | tupleNamesAffectIdentity | false estable | PR |
| TL-LANG-017 | CL-LANG-002 | tupleConversionExists con aridad desigual, identidad, builtin numerico y string | matriz booleana exacta sin conversion arbitraria | PR |
| TL-LANG-018 | CL-LANG-002 | computeWindowsX64AggregateLayout para vacio y campos validos con padding | kind/size/alignment/offsets exactos | PR |
| TL-LANG-019 | CL-LANG-002 | computeWindowsX64AggregateLayout con size cero, alignment invalido y overflow | nullopt sin layout parcial observable | PR |
| TL-LANG-020 | CL-LANG-002 | classifyWindowsX64AggregateReturn para 0..16 y valores grandes | direct solo 1,2,4,8; los demas indirectos segun tabla | PR |
| TL-LANG-021 | CL-LANG-002 | validateDeconstructionBindings con discard, underscore, nombres unicos, vacios y duplicados | primer rechazo exacto; discard no reserva nombre | PR |
| TL-LANG-022 | CL-LANG-002 | RecordDeriveSet vacio, flags aislados, union y contains | bitset exacto, inmutable y sin bits espurios | PR |
| TL-LANG-023 | CL-LANG-002 | validatePromotedFields con nombre/tipo validos, vacios y duplicados | aceptacion o primer error exacto | PR |
| TL-LANG-024 | CL-LANG-002 | validateAggregateSpread con 0,1,2 fuentes y campos explicitos unicos/duplicados | maximo una fuente y unicidad exacta | PR |

## Callables y closures — `CL-LANG-003`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LANG-025 | CL-LANG-003 | functionTypeIdentity para carriers, convenciones, flags, lifetimes, params, result y error | serializacion length-prefixed byte a byte | PR |
| TL-LANG-026 | CL-LANG-003 | parseFunctionTypeIdentity round-trip de matriz completa | reconstruye todos los campos sin perdida | PR |
| TL-LANG-027 | CL-LANG-003 | parseFunctionTypeIdentity con prefijo, flags, longitudes, count, separator y trailing invalidos | nullopt y consumo total obligatorio | PR |
| TL-LANG-028 | CL-LANG-003 | isFunctionTypeIdentity para identidades validas, truncadas y texto arbitrario | coincide exactamente con parse exitoso | PR |
| TL-LANG-029 | CL-LANG-003 | functionTypesAssignable variando un campo por vez | matriz direccional exacta para error/unsafe/noheap/nullability y igualdad del resto | PR |
| TL-LANG-030 | CL-LANG-003 | functionCarrierName y callingConventionName para todos los enums y externos | spelling o fallback vigente exacto | PR |
| TL-LANG-031 | CL-LANG-003 | anonymousLambdaTypeIdentity y parse para ordinals 0/max y signatures complejas | round-trip exacto | PR |
| TL-LANG-032 | CL-LANG-003 | anonymous lambda identity/parse con grammar invalida | nullopt sin aceptar trailing | PR |
| TL-LANG-033 | CL-LANG-003 | anonymousLambdaGeneratedName para ordinals de borde | `__nsc_lambda_O_L` exacto | PR |
| TL-LANG-034 | CL-LANG-003 | computeClosureLayout con vacio y captures de alignment mixto | size y alignment publicos exactos | PR |
| TL-LANG-035 | CL-LANG-003 | computeClosureLayout con nombre vacio, size cero, alignment invalido y overflow | falla con error exacto sin layout utilizable | PR |
| TL-LANG-036 | CL-LANG-003 | validateClosureEscape con no escape, by-value y referencias outlive/no-outlive | aceptacion o rechazo exacto | PR |
| TL-LANG-037 | CL-LANG-003 | callableTrait sobre ninguna, move, mutable-ref y combinaciones | prioridad FnOnce, luego FnMut, luego Fn | PR |
| TL-LANG-038 | CL-LANG-003 | nonCapturingLambdaConvertsToPointer y genericLambdaUsesErasedDispatch | conversion solo sin captures; generic erased false | PR |
| TL-LANG-039 | CL-LANG-003 | windowsX64BoundMethodLayout y validateMustTail sobre cleanup/ABI compatible/incompatible | layout 32/8 y prioridad de rechazo exacta | PR |

## Compatibilidad y migracion — `CL-LANG-004`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LANG-040 | CL-LANG-004 | classifyModuleCompatibility con contratos malformados, targets distintos y metadata major | clasificacion exacta y prioridad de incompatibilidad | PR |
| TL-LANG-041 | CL-LANG-004 | classifyModuleCompatibility con fingerprint igual/distinto | direct o adapter requerido exactamente | PR |
| TL-LANG-042 | CL-LANG-004 | validateCompilationPlan con 0,1,N unidades validas | vacio falla; grafo coherente acepta | PR |
| TL-LANG-043 | CL-LANG-004 | validateCompilationPlan variando module/source/fingerprint/identity/imports/duplicados | primer error exacto para cada invariante | PR |
| TL-LANG-044 | CL-LANG-004 | incrementalRebuildSet sin cambios, cambio hoja, cambio raiz y diamante | clausura transitiva determinista | PR |
| TL-LANG-045 | CL-LANG-004 | incrementalRebuildSet con modulo eliminado o nuevo | comportamiento vigente: removido ausente y nuevo incluido | PR |
| TL-LANG-046 | CL-LANG-004 | migrationDiagnosticCode para todos los issues y enum externo | MIG3001..MIG3007 y fallback MIG3000 | PR |
| TL-LANG-047 | CL-LANG-004 | mayApplyAutomaticFix con replacement/equivalencia en cuatro combinaciones | true solo con ambos requisitos | PR |
| TL-LANG-048 | CL-LANG-004 | classifyApiChange sobre 16 combinaciones de flags | clasificacion source/layout/ABI/private exacta | PR |
| TL-LANG-049 | CL-LANG-004 | validateMigrationGuide y removalAllowedInThreeX sobre orden, contenido, duplicados y dos justificaciones | aceptacion, diagnostico y tabla booleana exactos | PR |

## Coroutines y concurrencia — `CL-LANG-005`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LANG-050 | CL-LANG-005 | nativeFutureStart y asyncFrameHasImplicitHeap para todos los storages | lazy y ningun heap implicito | PR |
| TL-LANG-051 | CL-LANG-005 | borrowMayCrossAwait sobre cuatro combinaciones | true solo si outlives y address stable | PR |
| TL-LANG-052 | CL-LANG-005 | buildAsyncFunctionPlan sin awaits y locals no vivos/vivos | plan synchronous o error exacto | PR |
| TL-LANG-053 | CL-LANG-005 | buildAsyncFunctionPlan con awaits contiguos, huecos, duplicados y borrows validos/invalidos | plan exacto o primer rechazo | PR |
| TL-LANG-054 | CL-LANG-005 | buildAsyncFunctionPlan con return vacio, storage y self-reference | validacion y frame fields exactos | PR |
| TL-LANG-055 | CL-LANG-005 | validateFutureProtocol variando tipos, Poll/Resume y single-consumer | aceptacion o error exacto | PR |
| TL-LANG-056 | CL-LANG-005 | awaitAllowed en async/no-async, comptime y cleanup no-suspend | matriz completa y prioridad de rechazo | PR |
| TL-LANG-057 | CL-LANG-005 | FutureState::poll desde cada estado con Pending/Value/Error | transicion, completion y error exactos | PR |
| TL-LANG-058 | CL-LANG-005 | FutureState::consume desde cada estado | Ready consume una vez; demas rechazos exactos | PR |
| TL-LANG-059 | CL-LANG-005 | FutureState::cancel desde cada estado | Created/Pending cancelan; terminales no cambian | PR |
| TL-LANG-060 | CL-LANG-005 | cancellationCleanup con locals/defers vacios y N | orden contractual exacto | PR |
| TL-LANG-061 | CL-LANG-005 | buildAsyncCancellationPlan por cada suspended state y reason | defers por depth descendente, slots droppables reversos y causa conservada | PR |
| TL-LANG-062 | CL-LANG-005 | buildAsyncCancellationPlan con state/slot/defer invalidos | falla sin plan parcial | PR |
| TL-LANG-063 | CL-LANG-005 | asyncGeneratorMayProduce sobre todos los estados y demanda | matriz booleana exacta | PR |
| TL-LANG-064 | CL-LANG-005 | buildAsyncGeneratorPlan con suspensiones/yields validos | estados, frame, item/error/cancellation y flags exactos | PR |
| TL-LANG-065 | CL-LANG-005 | buildAsyncGeneratorPlan con ausencia de yield, huecos y borrow proofs invalidos | error exacto y nullopt | PR |
| TL-LANG-066 | CL-LANG-005 | AsyncGeneratorProtocolState::requestNext desde cada estado/demand flag | una demanda outstanding y terminalidad exactas | PR |
| TL-LANG-067 | CL-LANG-005 | AsyncGeneratorProtocolState::resume para todos los outcomes y estados | transiciones y errores exactos | PR |
| TL-LANG-068 | CL-LANG-005 | AsyncGeneratorProtocolState::takeItem y stopEarly desde cada estado | un item consumible y cancelacion estable | PR |
| TL-LANG-069 | CL-LANG-005 | structurallySend/Sync con value, raw pointer y negative impl | matriz exacta | PR |
| TL-LANG-070 | CL-LANG-005 | evaluateThreadSafety con value/atomic/shared/mutable/raw/dyn y overrides | decision, razones y prioridad exactas | PR |
| TL-LANG-071 | CL-LANG-005 | export/importThreadSafetyMetadata round-trip y metadata major/type invalidos | decision preservada o error exacto | PR |
| TL-LANG-072 | CL-LANG-005 | taskBorrowMayEscapeScope sobre cuatro combinaciones | true solo joined y outlives | PR |
| TL-LANG-073 | CL-LANG-005 | buildTaskScopePlan con executor completo/incompleto, noheap y children borrowed | plan o diagnostico exacto | PR |
| TL-LANG-074 | CL-LANG-005 | planTaskScopeExit normal/cancel/error con completed subset | cancel reverse, join reverse y propagation al final | PR |
| TL-LANG-075 | CL-LANG-005 | concurrency manifest/fingerprint repetidos | bytes y hash deterministas | PR |
| TL-LANG-076 | CL-LANG-005 | write/verify concurrency contract en path valido, parent ausente, CRLF y archivo alterado | creacion, normalizacion y diagnostico exactos | NIGHTLY |
| TL-LANG-077 | CL-LANG-005 | operaciones concurrentes independientes sobre state/metadata | no comparten estado global ni resultados | NIGHTLY |

## Lifetime entre modulos — `CL-LANG-006`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LANG-078 | CL-LANG-006 | makeCrossModuleLifetimeContract con module/type/allocator/policy | campos y tres thunks FNV-1a exactos | PR |
| TL-LANG-079 | CL-LANG-006 | validate contract valido y cada campo requerido vacio | exito o diagnostico exacto | PR |
| TL-LANG-080 | CL-LANG-006 | validate abi major/minor compatible e incompatible | major manda; minor queda en fingerprint | PR |
| TL-LANG-081 | CL-LANG-006 | crossModuleLifetimeFingerprint sobre cambios de cada campo | mismo input mismo hash; cada campo contractual afecta resultado | PR |
| TL-LANG-082 | CL-LANG-006 | registerObject primera vez y repeticiones | live count 1..N y fingerprint fijado | PR |
| TL-LANG-083 | CL-LANG-006 | registerObject con fingerprint distinto o unloading | rechazo exacto y count inmutable | PR |
| TL-LANG-084 | CL-LANG-006 | registerObject en overflow de contador | falla sin wrap | PR |
| TL-LANG-085 | CL-LANG-006 | unregisterObject valido, desconocido, fingerprint distinto y count cero | decremento exacto o rechazo sin mutacion | PR |
| TL-LANG-086 | CL-LANG-006 | requestUnload con 0 y N objetos | marca unloading solo a cero; live objects rechazan | PR |
| TL-LANG-087 | CL-LANG-006 | secuencias multihilo register/unregister/live/requestUnload | mutex preserva conteo y no mezcla contratos | NIGHTLY |

## Errores y control divergente — `CL-LANG-007`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LANG-088 | CL-LANG-007 | supportsQuestionPropagation con protocolo completo | acepta y no altera input | PR |
| TL-LANG-089 | CL-LANG-007 | supportsQuestionPropagation variando success/error y tres miembros | primer diagnostico exacto | PR |
| TL-LANG-090 | CL-LANG-007 | errorConversionIsExplicit identidad, diferencia y adapter | matriz booleana exacta | PR |
| TL-LANG-091 | CL-LANG-007 | validateThrowContract con error type y C ABI/adapters | aceptacion o error exacto | PR |
| TL-LANG-092 | CL-LANG-007 | catchesAllDeclaredErrors con subsets, duplicates y rethrow | rethrow cubre todo; set semantics exacta | PR |
| TL-LANG-093 | CL-LANG-007 | neverCoercesTo vacio y tipos representativos | false para vacio, true para destino no vacio | PR |
| TL-LANG-094 | CL-LANG-007 | unifyLoopBreakTypes con vacio, solo never, tipo unico, mezcla y conflicto | optional exacto ignorando never | PR |
| TL-LANG-095 | CL-LANG-007 | parsePanicProfile con spellings validos, case/trailing invalidos | perfil exacto o nullopt | PR |
| TL-LANG-096 | CL-LANG-007 | panicProfileName y panicContract para ambos perfiles | spelling, cleanup, unwind y FFI exactos | PR |
| TL-LANG-097 | CL-LANG-007 | assertionRequiresUnsafe para todos los kinds y externo | solo Assume true; fallback vigente caracterizado | PR |

## Expresiones e interpolacion — `CL-LANG-008`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LANG-098 | CL-LANG-008 | expressionLoweringContract para siete features | campos source-order/evaluate-once/allocation/ABI exactos | PR |
| TL-LANG-099 | CL-LANG-008 | expressionLoweringContract con enum externo | fallback/indexacion vigente aislada y caracterizada | NIGHTLY |
| TL-LANG-100 | CL-LANG-008 | expressionFeatureAvailable para todos los features | true estable | PR |
| TL-LANG-101 | CL-LANG-008 | isTypedParameterPackType con posiciones, vacio, cierre y trailing | clasificacion textual exacta | PR |
| TL-LANG-102 | CL-LANG-008 | classifyNullAwareOperand y supportsNullAwareOperators para pointer/reference/optional/other | kind y booleano coherentes | PR |
| TL-LANG-103 | CL-LANG-008 | resolveRangeEndpoint normal/from-end en 0,length,length+1 | indice o nullopt exacto | PR |
| TL-LANG-104 | CL-LANG-008 | resolveHalfOpenRange con defaults, endpoints y start mayor | resolved range o nullopt exacto | PR |
| TL-LANG-105 | CL-LANG-008 | parseStaticInterpolation con texto y holes simples | parts, offsets, expressions/formats exactos | PR |
| TL-LANG-106 | CL-LANG-008 | parseStaticInterpolation con llaves escapadas, nesting y quotes | segmentacion y unescape exactos | PR |
| TL-LANG-107 | CL-LANG-008 | parseStaticInterpolation con llaves, expresion, format o quote invalidos | error exacto y parts vacio | PR |
| TL-LANG-108 | CL-LANG-008 | renderStaticInterpolation con handler exitoso y fallo en cada part | orden, short-circuit y propagacion exactos | PR |
| TL-LANG-109 | CL-LANG-008 | builtinStaticInterpolationHoleSupported sobre bool/integer/format/width | matriz exacta | PR |
| TL-LANG-110 | CL-LANG-008 | renderBuiltinStaticInterpolation decimal, D/d/X/x, width, signo y -0 | bytes exactos y semantica vigente de signo | PR |
| TL-LANG-111 | CL-LANG-008 | render builtin con negativo hexadecimal, width>64 y tipo/formato invalido | falla con error exacto y result definido | PR |

## Genericos — `CL-LANG-009`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LANG-112 | CL-LANG-009 | normalizeConstraints con vacio, desorden, duplicados | orden, deduplicacion y canonical exactos | PR |
| TL-LANG-113 | CL-LANG-009 | normalizeConstraints con atom vacio y tres conflictos | primer error exacto y result definido | PR |
| TL-LANG-114 | CL-LANG-009 | validateConstGeneric con type/value presentes o vacios | aceptacion o error exacto | PR |
| TL-LANG-115 | CL-LANG-009 | constGenericArgumentMarker y parse round-trip con longitudes/bordes | campos exactos | PR |
| TL-LANG-116 | CL-LANG-009 | parse marker con prefijo, decimal, overflow y separator invalidos; valor normalizado como sufijo completo | rechazos dejan outputs definidos; un marker valido consume todo el sufijo como valor | PR |
| TL-LANG-117 | CL-LANG-009 | genericInstantiationIdentity con type/value args vacios y mixtos | identidad length-prefixed y orden exactos | PR |
| TL-LANG-118 | CL-LANG-009 | validatePackExpansion con 0/1/N packs, longitudes iguales/desiguales y budget | aceptacion o diagnostico exacto | PR |
| TL-LANG-119 | CL-LANG-009 | bidirectionalInferenceAccepts sobre igualdad/diferencia y arbitrary conversion | true solo igualdad sin conversion arbitraria | PR |
| TL-LANG-120 | CL-LANG-009 | aliasIdentity transparent/opaque con module/name/representation | identidad exacta segun kind | PR |
| TL-LANG-121 | CL-LANG-009 | specializationStrictlyDominates con equal/subset/superset/incomparable y duplicados | relacion de superconjunto estricto exacta | PR |
| TL-LANG-122 | CL-LANG-009 | InstantiationBudget enter/leave hasta depth/count y recursion | counters, maximumObservedDepth y errores exactos | PR |
| TL-LANG-123 | CL-LANG-009 | InstantiationBudget leave vacio y report stack >12 | no underflow; reporte truncado contractual | PR |

## Calidad de integracion — `CL-LANG-010`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LANG-124 | CL-LANG-010 | validateInteractionMatrix con matriz minima y seis pares criticos | acepta cuando identidad/seed/areas y cobertura son completas | PR |
| TL-LANG-125 | CL-LANG-010 | validateInteractionMatrix con identity vacia, areas<2, seed0 y duplicados | primer error exacto | PR |
| TL-LANG-126 | CL-LANG-010 | validatePrioritizedInteractionMatrix con Medium, sin impacto/minimization y sin triple | rechazos exactos | PR |
| TL-LANG-127 | CL-LANG-010 | buildNativeSharp3InteractionMatrix para seeds de borde | siete casos, IDs/areas/seeds deterministas | PR |
| TL-LANG-128 | CL-LANG-010 | minimizeInteractionCase con essential subset valido | caso minimo conserva identidad y campos contractuales | PR |
| TL-LANG-129 | CL-LANG-010 | minimizeInteractionCase con area ausente o menos de dos | nullopt y error exacto | PR |
| TL-LANG-130 | CL-LANG-010 | validateWholeProgramCorpus con corpus valido y cada invariante roto | aceptacion o primer error exacto | PR |
| TL-LANG-131 | CL-LANG-010 | build/validate canonical whole-program corpus | seis kinds, perfiles, exit42 y cuatro partial-noheap | PR |
| TL-LANG-132 | CL-LANG-010 | withinPerformanceBudget sobre cada metrica en debajo/igual/encima | comparacion inclusiva exacta | PR |
| TL-LANG-133 | CL-LANG-010 | nativeSharp3PerformanceBudgets y assessLanguagePerformance | seis perfiles, tolerancia, samples e identidad/hardware exactos | PR |
| TL-LANG-134 | CL-LANG-010 | diagnosticPassesQualityGate con codigo, idiomas, span, fixit, cascade y backtrace | aceptacion o error exacto | PR |
| TL-LANG-135 | CL-LANG-010 | validateDiagnosticQualityCatalog con cinco areas, duplicados y width | cobertura o diagnostico exacto | PR |
| TL-LANG-136 | CL-LANG-010 | renderDiagnosticForTerminal ingles/espanol y widths 0/1/N | bytes y truncamiento por bytes exactos | PR |
| TL-LANG-137 | CL-LANG-010 | semanticDebugName con owner/role/ordinal | identidad estable exacta | PR |
| TL-LANG-138 | CL-LANG-010 | validateSemanticDebugEvidence con ocho kinds, O0/O2, scopes y valueInvented | acepta evidencia completa; rechaza cada ausencia | PR |
| TL-LANG-139 | CL-LANG-010 | hiddenCostAuditPasses con costs explicitos/ocultos | primer costo oculto falla | PR |
| TL-LANG-140 | CL-LANG-010 | validateZeroCostAudit con records O0/O2, kinds y excepciones | aceptacion o diagnostico exacto | PR |
| TL-LANG-141 | CL-LANG-010 | semanticToolingSchemaVersion | `nsc-semantic-tooling-v1` exacto y estable | PR |
| TL-LANG-289 | CL-LANG-010 | `Console.ReadLine` con CRLF, línea vacía, 8192 caracteres y EOF en proceso real | contenido, estados y salida 42 exactos | PR |
| TL-LANG-290 | CL-LANG-010 | stdin real hacia `Console.ReadLine` y las diez fachadas numéricas | conversiones y fallback exactos; salida 42 | PR |

## Iteracion y generator ABI — `CL-LANG-011`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LANG-142 | CL-LANG-011 | validateIteratorProtocol con item/MoveNext/Current/cleanup | aceptacion y campos requeridos exactos | PR |
| TL-LANG-143 | CL-LANG-011 | validateIteratorProtocol lending por value/reference | lending solo por referencia | PR |
| TL-LANG-144 | CL-LANG-011 | lazyRangeCount con step positivo/negativo, inclusive/exclusive y vacio | count exacto sin materializacion | PR |
| TL-LANG-145 | CL-LANG-011 | lazyRangeCount con step cero, direccion contraria y overflow | nullopt y error exacto | PR |
| TL-LANG-146 | CL-LANG-011 | rangeMaterializesCollection y comprehensionHasImplicitAllocation para storages | false y matriz de allocation exacta | PR |
| TL-LANG-147 | CL-LANG-011 | mutationAllowedDuringBorrowedIteration para protocol lending/nonlending y mutation kinds | estructural prohibida solo durante lending | PR |
| TL-LANG-148 | CL-LANG-011 | computeGeneratorFrame con locals vivos/no vivos, alignment y self-reference | layout legacy exacto o nullopt | PR |
| TL-LANG-149 | CL-LANG-011 | buildSynchronousGeneratorPlan valido con states contiguos | frame, storage, item y suspensions exactos | PR |
| TL-LANG-150 | CL-LANG-011 | build generator con falta de yield, item conflict, huecos, intervalos y borrow invalidos | error exacto y nullopt | PR |
| TL-LANG-151 | CL-LANG-011 | generatorCancellationDropOrder por cada state | indices vivos/droppables en orden reverso | PR |
| TL-LANG-152 | CL-LANG-011 | generatorResumeState desde 0, yields, completed e invalido | siguiente state o nullopt exacto | PR |
| TL-LANG-153 | CL-LANG-011 | analyzeFusion para seis razones de rechazo en prioridad | decision y razon exactas | PR |
| TL-LANG-154 | CL-LANG-011 | mayFuse con contratos compatibles y cada incompatibilidad | booleano coherente con analyzeFusion | PR |

## Superficie historica — `CL-LANG-012`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LANG-155 | CL-LANG-012 | language3Surface | 16 entradas, orden, names, status y rationale exactos | PR |
| TL-LANG-156 | CL-LANG-012 | language3SurfaceEntry para todos los enums validos | referencia a la entrada correspondiente | PR |
| TL-LANG-157 | CL-LANG-012 | language3SurfaceEntry con enum externo | fallback vigente a primera entrada caracterizado | PR |
| TL-LANG-158 | CL-LANG-012 | language3SurfaceManifest | bytes completos, orden y labels historicas exactos | PR |
| TL-LANG-159 | CL-LANG-012 | language3SurfaceDiagnostic para Implemented | diagnostico exacto | PR |
| TL-LANG-160 | CL-LANG-012 | language3SurfaceDiagnostic para PlannedFor304, DeferredTo4 y RejectedFor3 | texto exacto y estado no productivo por disposition | PR |
| TL-LANG-161 | CL-LANG-012 | llamadas repetidas a surface/manifest/diagnostic | resultados deterministas y sin mutacion | PR |
| TL-LANG-294 | CL-LANG-012 | cuatro estados de inicializacion estatica y enum externo | orden, nombres y unknown exactos | PR |
| TL-LANG-295 | CL-LANG-012 | constructor estatico valido mas cinco formas invalidas | AST/simbolo/MIR static exactos y diagnosticos semanticos | PR |
| TL-LANG-296 | CL-LANG-012 | Producer referenciado por Consumer, ambos con constructor estatico | plan dependency-first, dependencia directa explicita y ownership exacto sin ejecutar al importar | PR |
| TL-LANG-297 | CL-LANG-012 | cctor explicito con campo estatico y grafo de modulos ciclico | CAS/body/publish, espera y reentrancia exactos; ciclo rechazado causalmente | PR |
| TL-LANG-298 | CL-LANG-012 | estados de inicializacion y perfiles panic abort/unwind-native | failed terminal sin retry y perfil panic exacto | PR |
| TL-LANG-299 | CL-LANG-012 | locales static escalar/propietario y negativos ref/VLA | storage/guard 0-1-2-3, sin cleanup lexico y destructor al unload condicionado a initialized | PR |
| TL-LANG-300 | CL-LANG-012 | declaraciones en if/while/switch y negativos de tipo, aridad y scope | bloque exacto por control, reinicializacion por iteracion y rechazo causal | PR |
| TL-LANG-301 | CL-LANG-012 | NSMETA 1.39, NSDEBUG 1.1, EXE/DLL, concurrencia y recursion T01 | round-trip/versiones exactas, PDB no vacio, EXE 42, DLL once/reentry/static-local/condition y unload | PR |
| TL-LANG-302 | CL-LANG-012 | cuerpos default en metodos, propiedades e indexadores de trait e interface | dispatch dinamico/ordinario produce 42 y abstract explicito con cuerpo se rechaza | PR |
| TL-LANG-303 | CL-LANG-012 | implementacion explicita, miembro nominal y defaults base/derivado en trait e interface | prioridad exacta produce 42 en cada ruta y el plan de tablas se obtiene sin error | PR |
| TL-LANG-304 | CL-LANG-012 | diamante ambiguo/resuelto, duplicado impl y productor/consumidor separados | rechazos causales exactos; override e impl local coherente producen 42 | PR |
| TL-LANG-305 | CL-LANG-012 | trait genérico con associated defaults, constraint, `some` y `dyn`; negativos object-safety | proyección estática, `dyn` y `some` producen 42; rechazos causales exactos | PR |
| TL-LANG-306 | CL-LANG-012 | productor static library con defaults de método/propiedad/indexador; consumidor sin fuentes del productor | NSMETA conserva 3 witnesses físicos y el consumidor termina 42 | PR |
| TL-LANG-307 | CL-LANG-012 | DLL debug con dos defaults y consumidor por constraint/`dyn` | slots/versionado, MIR/LLVM/PDB y paridad 21+21=42 | PR |

## Modelo de memoria — `CL-LANG-013`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LANG-162 | CL-LANG-013 | decodeMemoryOrder para 0..4,5,max | cinco valores o nullopt exacto | PR |
| TL-LANG-163 | CL-LANG-013 | validMemoryOrder Load sobre cinco orders | release/acq-rel rechazados | PR |
| TL-LANG-164 | CL-LANG-013 | validMemoryOrder Store sobre cinco orders | acquire/acq-rel rechazados | PR |
| TL-LANG-165 | CL-LANG-013 | validMemoryOrder RMW sobre cinco orders | todos aceptados | PR |
| TL-LANG-166 | CL-LANG-013 | validMemoryOrder Fence sobre cinco orders | relaxed rechazado y resto aceptado | PR |
| TL-LANG-167 | CL-LANG-013 | validCompareExchangeOrders sobre matriz 5x5 | failure load-valid y no mas fuerte que success | PR |
| TL-LANG-168 | CL-LANG-013 | dataRaceBehavior | UndefinedBehavior estable | PR |
| TL-LANG-169 | CL-LANG-013 | atomicAccessIsLockFreeWindowsX64 para widths 0,8,16,32,64,128 y align | true solo set y natural alignment | PR |
| TL-LANG-170 | CL-LANG-013 | volatileProvidesSynchronization y volatileAccessMayTear | false; tear para desalineado o >64 | PR |
| TL-LANG-171 | CL-LANG-013 | memoryModelManifest | bytes completos y deterministas | PR |

## Metaprogramacion — `CL-LANG-014`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LANG-172 | CL-LANG-014 | visibleIntrospectionMembers same/different module | filtrado public y orden sourceOrdinal exactos | PR |
| TL-LANG-173 | CL-LANG-014 | introspectionRequiresRuntimeMetadata | false estable | PR |
| TL-LANG-174 | CL-LANG-014 | validateAttributes con identities/targets validos y repeatable | acepta y conserva orden | PR |
| TL-LANG-175 | CL-LANG-014 | validateAttributes con vacios y duplicado no-repeatable | primer error exacto | PR |
| TL-LANG-176 | CL-LANG-014 | deriveDependencyKey con generator y fields desordenados/duplicados | orden canonical y bytes length-prefixed exactos | PR |
| TL-LANG-177 | CL-LANG-014 | sameBinding con spelling/scope iguales y distintos | booleano exacto | PR |
| TL-LANG-178 | CL-LANG-014 | capabilityAllowedInHermeticProfile para todos los enums | matriz declarada exacta | PR |
| TL-LANG-179 | CL-LANG-014 | comptimeInputIdentity con inputs desordenados y payloads | orden por identity e identidad exacta; duplicados caracterizados | PR |
| TL-LANG-180 | CL-LANG-014 | validPackageDiagnosticCode con package/code validos, prefijo ajeno y NS3 | matriz exacta | PR |
| TL-LANG-181 | CL-LANG-014 | ComptimeBudget consume en debajo/igual/encima de steps/bytes/depth | counters, no descuento depth y error exactos | PR |

## Modulos — `CL-LANG-015`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LANG-182 | CL-LANG-015 | canonicalWindowsModuleIdentity con case, slash, package/module vacios | concatenacion y normalizacion byte a byte exactas | PR |
| TL-LANG-183 | CL-LANG-015 | moduleGraphResolvable con grafo vacio, DAG lineal y diamante | true sin error | PR |
| TL-LANG-184 | CL-LANG-015 | moduleGraphResolvable con self-cycle, cycle N y dependencias Binding | cycle solo en Layout y diagnostico exacto | PR |
| TL-LANG-185 | CL-LANG-015 | moduleGraphResolvable con cadena profunda | comportamiento recursivo vigente caracterizado | NIGHTLY |
| TL-LANG-186 | CL-LANG-015 | normalizeImports con tipos, aliases y orden desordenado | resultado ordenado exacto | PR |
| TL-LANG-187 | CL-LANG-015 | normalizeImports con target vacio, alias invalido, duplicados y colision case-insensitive | primer error exacto; result definido | PR |
| TL-LANG-188 | CL-LANG-015 | canAccess para cuatro visibilities y combinaciones de flags | matriz booleana completa | PR |
| TL-LANG-189 | CL-LANG-015 | reexportedIdentity con original/facade vacio y no vacio | conserva identidad original segun contrato | PR |
| TL-LANG-190 | CL-LANG-015 | availabilityStatus con architecture match/mismatch y deprecated | prioridad de architecture y status exacta | PR |
| TL-LANG-191 | CL-LANG-015 | publicApiFingerprintInput con private/public y orden permutado | solo public, orden identity y bytes exactos | PR |

## Numericos y const memory — `CL-LANG-016`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LANG-192 | CL-LANG-016 | literalFits para tipos signed/unsigned 1..64 y magnitudes de borde | matriz booleana bit-exacta | PR |
| TL-LANG-193 | CL-LANG-016 | parseIntegerLiteralMagnitude decimal/hex/bin con separators | magnitud exacta | PR |
| TL-LANG-194 | CL-LANG-016 | parseIntegerLiteralMagnitude con octal, prefijo, digit, separator y overflow invalidos | fallo vigente y error exacto | PR |
| TL-LANG-195 | CL-LANG-016 | parseUntypedIntegerLiteral bases 2/8/10/16, suffixes y separators | base, digits, suffix y magnitude exactos | PR |
| TL-LANG-196 | CL-LANG-016 | parse untyped con input vacio, digit/base/suffix/separator/overflow invalidos | nullopt y error exacto | PR |
| TL-LANG-197 | CL-LANG-016 | resolveIntegerLiteral con signo, contextual type y selección por defecto | type/bits/negative exactos | PR |
| TL-LANG-198 | CL-LANG-016 | resolve literal fuera de rango y unsigned negativo | nullopt y diagnostico exacto | PR |
| TL-LANG-199 | CL-LANG-016 | addInteger sobre min/max y policies checked/wrapping/saturating/unchecked | optional y bits exactos | PR |
| TL-LANG-200 | CL-LANG-016 | evaluateIntegerArithmetic add/sub/mul sobre widths 1,8,16,32,64 | result/overflow/UB exactos | PR |
| TL-LANG-201 | CL-LANG-016 | evaluate arithmetic divide por cero y signed min/-1 | fallo, bits, overflow y UB segun policy | PR |
| TL-LANG-202 | CL-LANG-016 | evaluate shifts por 0,width-1,width,width+1 con policies | resultado o rechazo exacto | PR |
| TL-LANG-203 | CL-LANG-016 | integerResultAsSigned para sign bit y widths de borde | extension de signo exacta | PR |
| TL-LANG-204 | CL-LANG-016 | implicitNumericConversionIsLossless sobre matriz NumericType | matriz completa exacta | PR |
| TL-LANG-205 | CL-LANG-016 | classifyNumericConversion con/without constants y cuatro uses | kind/rank/requirement exactos | PR |
| TL-LANG-206 | CL-LANG-016 | floatingPointContract Strict/Precise/Fast | seis flags contractuales exactos | PR |
| TL-LANG-207 | CL-LANG-016 | strongNumericHasRuntimeWrapper y implicit conversion identities | false y conversion solo misma identity | PR |
| TL-LANG-208 | CL-LANG-016 | buildStrongNumericPlan con definicion valida | plan representation/traits/FFI exacto | PR |
| TL-LANG-209 | CL-LANG-016 | build strong numeric con identity/fields/width/transparent/copy-move invalidos | primer error exacto | PR |
| TL-LANG-210 | CL-LANG-016 | strongNumericOperatorAllowed sobre operator sets e identities | booleano exacto | PR |
| TL-LANG-211 | CL-LANG-016 | ConstantMemory allocate con size/alignment validos e invalidos | provenance monotono o error exacto | PR |
| TL-LANG-212 | CL-LANG-016 | ConstantMemory offset/dereference en inicio/final/fuera/deallocated | pointer o diagnostico exacto sin wrap | PR |
| TL-LANG-213 | CL-LANG-016 | ConstantMemory deallocate valido, desconocido y doble free | estado y error exactos | PR |
| TL-LANG-214 | CL-LANG-016 | constant union activate/read same/different/vacio | active member y errores exactos | PR |
| TL-LANG-215 | CL-LANG-016 | evaluateConstantIntegerArithmetic y loop con budgets/operations/policies | mismo contrato aritmetico y corte exacto | PR |
| TL-LANG-216 | CL-LANG-016 | integerSemanticsSelfCheck | true para barrida completa signed i8 | NIGHTLY |
| TL-LANG-287 | CL-LANG-016 | `Parse` directo y fallback para byte/sbyte/short/ushort/int/uint/long/ulong/float/double | valores y límites exactos; salida 42 | PR |
| TL-LANG-288 | CL-LANG-016 | `Int32.Parse` directo con `string null` | frontera `panic` determinista, salida 142 | PR |
| TL-LANG-291 | CL-LANG-016 | unidad con Number/NumericLimits/NumericParsing: mínimos, máximos y adyacentes | aceptación/rechazo exactos; salida 42 | PR |
| TL-LANG-292 | CL-LANG-016 | unidad con `null` para diez `Number.TryParse*` y out previamente no cero | false y out cero tipado; salida 42 | PR |
| TL-LANG-293 | CL-LANG-016 | unidad de fachadas `Parse`, fallback y `TryParse` sin proyecto Core completo | delegación, valores y fallback exactos; salida 42 | PR |

## Ownership — `CL-LANG-017`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LANG-217 | CL-LANG-017 | moveWholeValue Copy con estados de campos variados | no-op exitoso | PR |
| TL-LANG-218 | CL-LANG-017 | moveWholeValue non-copy con 0,1,N initialized | falla si ninguno; apaga todos si alguno | PR |
| TL-LANG-219 | CL-LANG-017 | moveField con indice valido inicializado | apaga solo el campo y conserva resto | PR |
| TL-LANG-220 | CL-LANG-017 | moveField fuera de rango, ya movido y custom destructor | falla con error exacto sin mutacion extra | PR |
| TL-LANG-221 | CL-LANG-017 | classifyValueState para todos true, todos false y mezcla | Available/Moved/PartiallyMoved exactos | PR |
| TL-LANG-222 | CL-LANG-017 | requiredDropFlagCount para estados Copy/non-copy/partial | count contractual exacto | PR |
| TL-LANG-223 | CL-LANG-017 | borrowsOverlap con roots distintos, iguales y projections prefijo/divergente | matriz exacta | PR |
| TL-LANG-224 | CL-LANG-017 | mayTakeExclusiveBorrows sobre overlap | negacion contractual exacta | PR |
| TL-LANG-225 | CL-LANG-017 | inferElidedOutputLifetime con receiver, 0,1,N inputs | receiver o unico input; ambiguity nullopt | PR |
| TL-LANG-226 | CL-LANG-017 | outlives con pares presentes/ausentes y ordenes | resultado depende de primera coincidencia exacta | PR |
| TL-LANG-227 | CL-LANG-017 | cleanupExitOrder con 0,1,N actions | inversion exacta | PR |
| TL-LANG-228 | CL-LANG-017 | operationPreservesPin con borrow/projection/destroy y otros | true solo tres spellings | PR |
| TL-LANG-229 | CL-LANG-017 | encode/decode EscapeSummary round-trip con classes 0,N | bytes y estructura exactos | PR |
| TL-LANG-230 | CL-LANG-017 | decode escape summary con version/length/separator/class/trailing invalidos | false, error exacto y output reiniciado | PR |

## Release y promocion — `CL-LANG-018`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LANG-231 | CL-LANG-018 | validateFreezeManifest completo y cada revision vacia | aceptacion o error exacto | PR |
| TL-LANG-232 | CL-LANG-018 | validateSyntaxFreeze con hashes/revisions/gates | acepta completo; rechaza cada ausencia | PR |
| TL-LANG-233 | CL-LANG-018 | validateSemanticFreeze con milestones 3.1/3.2/4 y otros | prefijos permitidos y diagnostico exactos | PR |
| TL-LANG-234 | CL-LANG-018 | validateWin64AbiFreeze con target windows-x64 y variantes | target exacto y evidencias requeridas | PR |
| TL-LANG-235 | CL-LANG-018 | validateConformanceManifest con IDs, tiers y duplicados | al menos Mandatory, IDs unicos y errores exactos | PR |
| TL-LANG-236 | CL-LANG-018 | validateCandidateConformance con tiers/evidencias completas e incompletas | tres tiers y cuatro evidencias obligatorias | PR |
| TL-LANG-237 | CL-LANG-018 | bilingualSpecificationInParity con IDs/requirements/examples | booleano y error exactos | PR |
| TL-LANG-238 | CL-LANG-018 | validateBilingualSpecificationAudit con notes/glossary/links/samples/indexes | aceptacion o primera ausencia exacta | PR |
| TL-LANG-239 | CL-LANG-018 | changeAllowed sobre matriz 3 stages x severities | RC1 none, RC2 fix/blocker, RC3 blocker | PR |
| TL-LANG-240 | CL-LANG-018 | validateReleaseCandidate sin cambios y con cambio permitido | acepta y conserva gates | PR |
| TL-LANG-241 | CL-LANG-018 | validateReleaseCandidate con cambio aceptado y gates no reiniciados | rechaza con diagnostico exacto | PR |
| TL-LANG-242 | CL-LANG-018 | promotionStatus con todas las combinaciones de flags esenciales | status exacto | PR |
| TL-LANG-243 | CL-LANG-018 | missingPromotionEvidence con cada evidencia ausente | lista completa en orden estable | PR |
| TL-LANG-244 | CL-LANG-018 | officialPromotionStatus valido | Promotable y missing vacio | PR |
| TL-LANG-245 | CL-LANG-018 | official promotion con hashes/branch/boundaries/smoke/install/samples/artifacts ausentes | status y lista ordenada exactos | PR |
| TL-LANG-246 | CL-LANG-018 | stableLanguageVersion y normativeTarget | `3.0.0` y `windows-x64` exactos | PR |
| TL-LANG-247 | CL-LANG-018 | llamadas repetidas a manifests/status | resultados deterministas y sin estado global | PR |
| TL-LANG-248 | CL-LANG-018 | valores historicos de version/branch frente a politica posterior | caracterizados como contrato vigente, no reinterpretados | PR |

## Smart pointers — `CL-LANG-019`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LANG-249 | CL-LANG-019 | smartPointerFamilyContract para cinco familias | nombre/thread/null/copy/clone/weak exactos | PR |
| TL-LANG-250 | CL-LANG-019 | smartPointerFamilyContract con enum externo | indexacion vigente aislada y caracterizada | NIGHTLY |
| TL-LANG-251 | CL-LANG-019 | validate operation Move para todas las familias | siempre acepta | PR |
| TL-LANG-252 | CL-LANG-019 | validate implicit Copy para todas las familias | siempre rechaza con error exacto | PR |
| TL-LANG-253 | CL-LANG-019 | validate Clone/Downgrade/Upgrade por familia soportada/no soportada | aceptacion o diagnostico exacto | PR |
| TL-LANG-254 | CL-LANG-019 | upgrade con strong count cero/max y resurrection | reglas de no resurrection/overflow exactas | PR |
| TL-LANG-255 | CL-LANG-019 | Release/Reset en Unique y otras familias | solo Unique acepta | PR |
| TL-LANG-256 | CL-LANG-019 | cross-thread con Rc/WeakRc, payload Send/Sync y producer pinned | matriz de restricciones exacta | PR |
| TL-LANG-257 | CL-LANG-019 | smartPointerContractManifest | bytes completos y orden exacto | PR |
| TL-LANG-258 | CL-LANG-019 | fingerprint repetido y ante cambio de manifest | determinista y sensible a bytes | PR |
| TL-LANG-259 | CL-LANG-019 | write contract con parent existente/ausente y fallo filesystem | archivo o diagnostico exacto | NIGHTLY |
| TL-LANG-260 | CL-LANG-019 | verify contract exacto, CRLF y alterado | normaliza CRLF; detecta cualquier diferencia restante | NIGHTLY |

## Sum types y patterns — `CL-LANG-020`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LANG-261 | CL-LANG-020 | compute variant layout con 1,255,256,65535,65536 cases | tag 1/2/4 y layout exactos | PR |
| TL-LANG-262 | CL-LANG-020 | compute variant layout con names vacios, alignment invalido y overflow | nullopt sin layout parcial | PR |
| TL-LANG-263 | CL-LANG-020 | variant layout con payloads de size/alignment mixtos | offsets, max payload, size y alignment exactos | PR |
| TL-LANG-264 | CL-LANG-020 | orPatternBindingsCompatible con maps iguales en distinto orden | true | PR |
| TL-LANG-265 | CL-LANG-020 | or patterns con count/name/type/mode distintos | rechazo y error exactos | PR |
| TL-LANG-266 | CL-LANG-020 | or patterns con duplicado en alternativa izquierda | comportamiento emplace vigente caracterizado | PR |
| TL-LANG-267 | CL-LANG-020 | analyzeClosedMatch con exact coverage, missing, wildcard y guarded arms | missing/unreachable/exhaustive exactos | PR |
| TL-LANG-268 | CL-LANG-020 | letElseGuaranteesInitialization false/true | igual a elseDiverges | PR |
| TL-LANG-269 | CL-LANG-020 | canReadCUnionMember sobre cuatro combinaciones | true con proof o unsafe context | PR |
| TL-LANG-270 | CL-LANG-020 | variantMetadataIdentity con 0,1,N cases y payloads distintos | qualified name y case names; payload no afecta identidad | PR |
| TL-LANG-308 | CL-LANG-020 | array nativo con list patterns exacto, prefijo, sufijo y `_` | length-first, elementos ordenados y salida 42 | PR |
| TL-LANG-309 | CL-LANG-020 | vista prestada con `[10, .. var middle, 40]` | `middle` conserva el origen mediante `Slice`, no copia elementos y produce salida 52 | PR |

## Traits y operadores — `CL-LANG-021`

| ID | Contrato | Entrada/preparación | Oráculo | Frecuencia |
|---|---|---|---|---|
| TL-LANG-271 | CL-LANG-021 | validateTrait con identity, storage y signatures validos | acepta | PR |
| TL-LANG-272 | CL-LANG-021 | validateTrait con identity/signature vacios, storage y duplicados | primer error exacto | PR |
| TL-LANG-273 | CL-LANG-021 | validateImplCoherence con ownership trait/type por modulo | aceptacion exacta | PR |
| TL-LANG-274 | CL-LANG-021 | validate coherence con impl huerfano y duplicado trait+type | rechazo exacto independiente del module string candidato | PR |
| TL-LANG-275 | CL-LANG-021 | normalizeAssociatedType con cadena aciclica | resultado terminal exacto | PR |
| TL-LANG-276 | CL-LANG-021 | normalize associated type con key duplicada y ciclo | falla con error exacto | PR |
| TL-LANG-277 | CL-LANG-021 | validateOpaqueResultTypes con vacio, uno, iguales y distintos | aceptacion o diagnostico exacto | PR |
| TL-LANG-278 | CL-LANG-021 | isObjectSafe con methods normales, generic y Self by value | matriz y primer error exactos | PR |
| TL-LANG-279 | CL-LANG-021 | windowsX64TraitObjectLayout | {8,8,descriptor-map,null-zero,no-allocation} exacto | PR |
| TL-LANG-280 | CL-LANG-021 | dispatchMetadata static/dyn e identity vacia/no vacia | spelling exacto | PR |
| TL-LANG-281 | CL-LANG-021 | resolveExtension con instance y 0/1/N extensions | instance/missing/unique/ambiguous exactos | PR |
| TL-LANG-282 | CL-LANG-021 | operatorTraitContracts | 20 filas, orden, spelling, arity, trait y method exactos | PR |
| TL-LANG-283 | CL-LANG-021 | operatorTraitContract por spelling+arity para todas las filas | round-trip al registro correspondiente | PR |
| TL-LANG-284 | CL-LANG-021 | operatorTraitContractForMethod por method+arity para todas las filas | round-trip inverso exacto | PR |
| TL-LANG-285 | CL-LANG-021 | operatorTrait/operatorMethodName overloads exactos y unknown | proyecciones exactas; overload legacy prefiere binary | PR |
| TL-LANG-286 | CL-LANG-021 | operatorCanBeOverloaded para 20 spellings, `&&`, `\|\|`, `??` y unknown | true solo tabla contractual | PR |

## Criterios de cierre futuro

1. Cada fila debe implementarse completa o quedar bloqueada con evidencia; diseñarla no prueba nada.
2. Cada matriz enumera todos sus valores y bordes, no una muestra representativa.
3. Los casos que caracterizan comportamiento riesgoso vigente se aíslan; no se reinterpretan como aprobación.
4. Los writers usan directorios temporales propios y comparan bytes completos y estado posterior.
5. Las máquinas de estado prueban cada operación desde cada estado alcanzable y terminal.
6. Coverage instrumental y mutation testing pertenecen a Fase 3.

**Total diseñado:** 293 casos lógicos parametrizables.
