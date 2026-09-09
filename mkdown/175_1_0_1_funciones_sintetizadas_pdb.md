# 1.0.1 - Funciones sintetizadas visibles en PDB / Synthesized functions visible in PDB

## Resultado / Outcome

**ES.** El perfil `-g` publica ahora como procedimientos CodeView/PDB estandar las
rutinas internas que ejecutan inicializacion estatica, destruccion de objetos,
filtrado tipado de excepciones y destruccion de contextos de closures. Conservan
nombre legible, linkage exacto, firma ABI y ubicacion fuente de origen en O0 y O2.

**EN.** The `-g` profile now publishes the internal routines that execute static
initialization, object destruction, typed exception filtering, and closure-context
destruction as standard CodeView/PDB procedures. They retain a readable name,
exact linkage, ABI signature, and originating source location at O0 and O2.

El debugger de `A:\native-sharp-debugger` se mantuvo estrictamente en modo de solo
lectura y no fue modificado.

The debugger at `A:\native-sharp-debugger` remained strictly read-only and was not
modified.

## Clasificacion estandar / Standard classification

**ES.** Una rutina sintetizada con comportamiento propio se marca
`DIFlagArtificial`, pero no `DIFlagThunk`. LLVM la serializa como `S_GPROC32`, lo
que permite mostrarla, resolver su direccion y recorrer su frame sin presentarla
como codigo escrito por el usuario.

**EN.** A synthesized routine with behavior of its own is marked
`DIFlagArtificial`, but not `DIFlagThunk`. LLVM serializes it as `S_GPROC32`, which
allows a debugger to show it, resolve its address, and walk its frame without
presenting it as user-authored code.

Las fronteras de adaptacion ABI ya existentes —wrapper `export extern C` y guard de
excepciones de callback— conservan `Artificial | Thunk` y siguen apareciendo como
`S_THUNK32`. La diferencia queda expresada por registros CodeView estandar, no por
un sidecar o protocolo privado.

The existing ABI adaptation boundaries —the `export extern C` wrapper and callback
exception guard— retain `Artificial | Thunk` and remain `S_THUNK32`. The distinction
is expressed through standard CodeView records, not a private sidecar or protocol.

## Archivos modificados / Files modified

### `src/LLVM/ProjectOopLlvmLowerer.cpp`

#### `createSynthesizedDebugSubprogram`

- **ES:** centraliza la creacion de un `DISubprogram` sintetizado a partir de rango
  fuente, nombre visible, linkage, retorno, parametros y clasificacion thunk/no
  thunk.
- **EN:** centralizes synthesized `DISubprogram` creation from source range,
  display name, linkage, return type, parameters, and thunk/non-thunk
  classification.
- **ES:** construye la firma mediante `debugType()` y
  `LLVMDIBuilderCreateSubroutineType`; no reutiliza una firma semantica distinta
  cuando el helper tiene un ABI propio.
- **EN:** builds the signature through `debugType()` and
  `LLVMDIBuilderCreateSubroutineType`; it does not reuse a different semantic
  signature when the helper has its own ABI.

#### `createGeneratedDebugSubprogram`

- **ES:** pasa a delegar en el helper comun y mantiene `Artificial | Thunk` para
  wrappers C y limites de callbacks.
- **EN:** now delegates to the common helper and preserves `Artificial | Thunk`
  for C wrappers and callback boundaries.

#### `debugSubprogramLocation`

- **ES:** agrega una variante por `SourceRange` para asignar ubicaciones estandar
  tambien cuando no existe una funcion MIR de usuario uno-a-uno.
- **EN:** adds a `SourceRange` variant so standard locations can also be assigned
  when there is no one-to-one user MIR function.

#### `declareTypeInitializers` y `defineTypeInitializers`

- **ES:** cada `__nsc_cctor_N` recibe el nombre
  `<Tipo>.cctor [generated]`, firma `void ()`, linkage real y la declaracion del
  tipo como origen.
- **EN:** every `__nsc_cctor_N` receives the name
  `<Type>.cctor [generated]`, a `void ()` signature, real linkage, and the type
  declaration as its origin.
- **ES:** el scope DI permanece activo mientras se bajan los inicializadores de
  campos, por lo que sus expresiones conservan lineas fuente dentro del cctor.
- **EN:** the DI scope remains active while field initializers are lowered, so
  their expressions retain source lines inside the cctor.

#### `declareDestructionEntries` y `defineDestructionEntries`

- **ES:** cada entrada de destruccion recibe
  `<Tipo>.destruction-entry [generated]`, firma `void (objeto)` y el linkage exacto
  almacenado en el descriptor de clase.
- **EN:** every destruction entry receives
  `<Type>.destruction-entry [generated]`, a `void (object)` signature, and the
  exact linkage stored in the class descriptor.
- **ES:** la ubicacion sintetica abarca el recorrido contractual de destructores,
  liberacion de campos owned y liberacion final del objeto.
- **EN:** the synthesized location spans the contractual destructor walk, owned
  field release, and final object release.

#### `defineTypedCatchFilters`

- **ES:** cada `__nativesharp_eh_filter_type_*` se publica como
  `<TipoExcepcion>.catch-filter [generated]` con firma ABI `int (void*, void*)`.
- **EN:** every `__nativesharp_eh_filter_type_*` is published as
  `<ExceptionType>.catch-filter [generated]` with ABI signature
  `int (void*, void*)`.
- **ES:** la declaracion del tipo capturado proporciona archivo y linea estables,
  ya que un filtro se comparte entre todos los catch del mismo tipo.
- **EN:** the caught type declaration provides stable file and line identity,
  because one filter is shared by all catches of the same type.

#### `closureContextDestroy`

- **ES:** resuelve la lambda MIR propietaria a partir del linkage del callee y
  publica `<Metodo>.lambda#N [generated: closure context destroy]` con firma
  `void (void*)`.
- **EN:** resolves the owning MIR lambda from the callee linkage and publishes
  `<Method>.lambda#N [generated: closure context destroy]` with signature
  `void (void*)`.
- **ES:** la ubicacion DI se destruye antes que el builder LLVM auxiliar; esto
  mantiene valida la disciplina RAII aun cuando el helper usa un builder separado.
- **EN:** the DI location is destroyed before the auxiliary LLVM builder; this
  keeps RAII valid even though the helper uses a separate builder.

### `tests/integration/debug_pdb/Main.ns`

El fixture agrega, sin dependencias externas:

The fixture adds, without external dependencies:

- `GeneratedState.Seed`, que fuerza un inicializador de tipo / which forces a type
  initializer;
- `Resource.~Resource` y un local `raii`, que fuerzan la entrada de destruccion /
  which force the destruction entry;
- `DebugProblem` y un `catch` tipado, que fuerzan el filtro EH / which force the
  EH filter;
- una lambda con captura materializada como `Reader`, que fuerza el destructor de
  contexto / a capturing lambda materialized as `Reader`, which forces the context
  destructor.

`Main()` ejecuta todas las rutas no excepcionales y conserva el resultado 42.

`Main()` executes all non-exceptional paths and retains result 42.

### `cmake/VerifyNativeSharpDebugArtifacts.cmake`

- **ES:** ejecuta los binarios de prueba O0 y O2 y exige codigo de salida 42.
- **EN:** executes the O0 and O2 fixture binaries and requires exit code 42.
- **ES:** exige `S_GPROC32` para cctor, entrada de destruccion, filtro tipado y
  destructor de contexto.
- **EN:** requires `S_GPROC32` for the cctor, destruction entry, typed filter, and
  context destructor.
- **ES:** mantiene las dos aserciones `S_THUNK32` anteriores para demostrar que la
  nueva clasificacion no degrada las fronteras ABI.
- **EN:** retains the two previous `S_THUNK32` assertions to prove that the new
  classification does not degrade ABI boundaries.
- **ES:** actualiza las lineas fuente de control a 45 y 105 despues de ampliar el
  fixture.
- **EN:** updates the source-line sentinels to 45 and 105 after expanding the
  fixture.

### `spec/NativeSharp_Debug_Artifact_Contract_1_0.md`

**ES.** Formaliza que las rutinas internas con comportamiento son artificiales,
pero no thunks, mientras que las fronteras de adaptacion ABI llevan ambas marcas.
Exige que la distincion sobreviva en el tipo de registro CodeView/PDB.

**EN.** Formalizes that internal routines with behavior are artificial but not
thunks, while ABI adaptation boundaries carry both marks. It requires the
distinction to survive in the CodeView/PDB record kind.

## Evidencia dirigida / Targeted evidence

- Gate PDB manual O0/O2: `NativeSharp O0/O2 CodeView and PDB artifacts verified`.
- Ejecucion manual / Manual execution: O0 `42`, O2 `42`.
- `nsc_project_oop_lowering_tests`: `PASS: 71 checks`.
- `nsc_stage37_static_initialization_tests`: `PASS: 60 checks`.
- `nsc_stage45_11_eh_cleanup_execution_tests`: `PASS: 72 checks`.
- `nsc_stage50_11_ownership_execution_tests`: `PASS: 196 checks`.
- `nsc_stage67_e11_lambda_execution_tests`: `PASS: 4 checks`.

## Baseline global / Global baseline

**ES.** El baseline global se ejecuto una sola vez al cierre del lote con el
compilador construido e instalado desde el estado actual del disco:

- build instalado: `b86d060dc8b87183`;
- CTest `lld-link`: `307/307`;
- fixtures `lld-link`: `148/148`;
- triage `lld-link`: `18/18`;
- CTest `NSLink`: `307/307`;
- fixtures `NSLink`: `148/148`;
- triage `NSLink`: `18/18`;
- Gate 0: `LANDED`;
- duracion total: `1541.36 s`;
- corrida: `A:\\nsc-devel\\_nscbuilder\\history\\20260909-155950-ff92fdc8`.

**EN.** The global baseline was run exactly once at batch closure with the
compiler built and installed from the current on-disk state:

- installed build: `b86d060dc8b87183`;
- `lld-link` CTest: `307/307`;
- `lld-link` fixtures: `148/148`;
- `lld-link` triage: `18/18`;
- `NSLink` CTest: `307/307`;
- `NSLink` fixtures: `148/148`;
- `NSLink` triage: `18/18`;
- Gate 0: `LANDED`;
- total duration: `1541.36 s`;
- run: `A:\\nsc-devel\\_nscbuilder\\history\\20260909-155950-ff92fdc8`.

### `_abi/gates/G00/gate.json`

**ES.** Actualiza la evidencia certificada de Gate 0 con el identificador de la
corrida, el digest del estado fuente y los hashes de los binarios producidos e
instalados por este baseline.

**EN.** Updates the certified Gate 0 evidence with the run identifier, source
state digest, and hashes of the binaries produced and installed by this baseline.

## Pendientes PDB / Remaining PDB work

Este lote no declara cerrado todo `NSC-DEBUG-004`. Permanecen las tablas/thunks de
interfaz, representacion detallada de funclets SEH, checksums de fuente, remapeo de
rutas y reproducibilidad final.

This batch does not declare all of `NSC-DEBUG-004` complete. Interface tables and
thunks, detailed SEH funclet representation, source checksums, path remapping, and
final reproducibility remain.

## Mensaje de commit / Commit message

### Titulo ES / Spanish title

`Expone funciones sintetizadas en PDB`

### Title EN / English title

`Expose synthesized functions in PDB`

### Descripcion ES / Spanish description

`Publica inicializadores estaticos, entradas de destruccion, filtros EH tipados y destructores de contextos de closures como procedimientos CodeView artificiales con firma y origen, preserva los thunks ABI y amplia el gate O0/O2 con ejecucion.`

### Description EN / English description

`Publish static initializers, destruction entries, typed EH filters, and closure-context destructors as artificial CodeView procedures with signatures and origins, preserve ABI thunks, and expand the O0/O2 gate with execution.`
