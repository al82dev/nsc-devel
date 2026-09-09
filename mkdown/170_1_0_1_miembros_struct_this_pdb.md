# 170 — PDB extremo: structs, `this`, scopes y thunks / Extreme PDB: structs, `this`, scopes and thunks

## Resultado / Outcome

El perfil completo `-g` publica los campos de structs NativeSharp con sus nombres, tipos,
accesibilidad y offsets reales. Los métodos de instancia publican además `this` ligado al receptor
ABI que ejecuta el código. Los bloques MIR se convierten en scopes léxicos PDB y las fronteras
generadas para callbacks y exports C se clasifican como thunks estándar. La información sigue
siendo CodeView/PDB estándar y funciona en O0 y O2.

The full `-g` profile publishes NativeSharp struct fields with their real names, types,
accessibility and offsets. Instance methods also publish `this`, bound to the ABI receiver that
executes the code. MIR blocks become lexical PDB scopes, and generated callback/C-export
boundaries are classified as standard thunks. The information remains standard CodeView/PDB and
works at O0 and O2.

Este lote completa cuatro pendientes inmediatos declarados en el documento 169: miembros de
structs, `this`, ámbitos léxicos anidados y clasificación de las dos fronteras generadas que
cruzan código nativo. `NSC-DEBUG-004` conserva trabajo posterior para otras familias internas.

This batch completes four immediate items left by document 169: struct members, `this`, nested
lexical scopes, and classification of the two generated boundaries that cross native code.
`NSC-DEBUG-004` still has later work for other internal families.

## Archivos modificados / Files modified

### `include/nativesharp/Frontend/ProjectOopMirModel.h`

`ProjectOopMirStructLayout` transporta ahora `fields`, usando directamente
`ObjectFieldLayout`. Cada entrada conserva el ordinal semántico del miembro, tipo, offset, tamaño
y alineación calculados por `ProjectObjectLayoutModel`.

`ProjectOopMirStructLayout` now carries `fields` directly as `ObjectFieldLayout` entries. Each
entry preserves the semantic member ordinal, type, offset, size and alignment computed by
`ProjectObjectLayoutModel`.

### `src/Frontend/ProjectOopMirModel.cpp`

La construcción MIR copia `StructValueLayout::fields` junto con el layout exterior. El backend no
recalcula padding ni reconstruye offsets leyendo declaraciones.

MIR construction copies `StructValueLayout::fields` together with the outer layout. The backend
does not recalculate padding or reconstruct offsets from declarations.

### `src/LLVM/LlvmCApi.h`

Amplía el subconjunto LLVM 23 con las firmas oficiales para:

- flags de accesibilidad, `Artificial`, `ObjectPointer` y `Thunk`;
- `LLVMDIBuilderCreateLexicalBlock`;
- `LLVMDIBuilderCreateMemberType`;
- `LLVMDIBuilderCreateReplaceableCompositeType`;
- `LLVMMetadataReplaceAllUsesWith`;
- el tag DWARF oficial de `structure_type`.

Expands the LLVM 23 subset with the official signatures for:

- accessibility, `Artificial`, `ObjectPointer`, and `Thunk` flags;
- `LLVMDIBuilderCreateLexicalBlock`;
- `LLVMDIBuilderCreateMemberType`;
- `LLVMDIBuilderCreateReplaceableCompositeType`;
- `LLVMMetadataReplaceAllUsesWith`;
- the official DWARF `structure_type` tag.

### `src/LLVM/ProjectOopLlvmLowerer.cpp`

#### `debugMemberFlags`

Traduce accesibilidad NativeSharp a los atributos DI estándar. `internal` se conserva sin fingir
una categoría C++ que CodeView no representa directamente.

Maps NativeSharp accessibility to standard DI attributes. `internal` remains unqualified instead
of pretending it is a C++ access category that CodeView cannot represent directly.

#### `debugType`

Para cada struct:

1. crea un tipo compuesto reemplazable y lo introduce inmediatamente en el cache;
2. crea cada miembro con nombre semántico, tipo, archivo/línea, accesibilidad, tamaño, alineación
   y offset MIR;
3. crea el tipo completo;
4. reemplaza atómicamente el placeholder mediante RAUW.

Esto permite que un campo puntero refiera al tipo que se está construyendo sin recursión
infinita ni tipos duplicados. El PDB resultante usa `LF_FIELDLIST`/`LF_MEMBER` normales.

For each struct it:

1. creates a replaceable composite and immediately enters it into the cache;
2. creates every member with semantic name, type, file/line, accessibility, size, alignment and
   MIR offset;
3. creates the complete type;
4. atomically replaces the placeholder through RAUW.

This lets a pointer field refer to the type being built without infinite recursion or duplicate
types. The resulting PDB uses normal `LF_FIELDLIST`/`LF_MEMBER` records.

#### `emitDebugThis`

Publica `this` como variable implícita artificial del método de instancia. Para structs enlaza el
almacenamiento apuntado por el receptor; para tipos referencia crea un hogar local para el valor
puntero y deja que LLVM determine sus rangos finales. Lambdas y métodos estáticos no inventan
`this`.

Publishes `this` as the artificial implicit variable of an instance method. For structs it binds
the storage addressed by the receiver; for reference types it creates a local home for the pointer
value and lets LLVM determine final ranges. Lambdas and static methods do not fabricate `this`.

#### `ScopedDebugScope`, `debugLocation` y `emitDebugLocal`

Cada nodo MIR `Block` crea un `DILexicalBlock` anidado bajo el scope vigente. Un guard RAII restaura
el padre aun cuando el lowering sale por error. Las ubicaciones y variables automáticas consumen
el scope activo; los parámetros y `this` permanecen correctamente en el scope del procedimiento.
Para archivos secundarios se crea un `DILexicalBlockFile` bajo el scope actual, sin reutilizar el
antiguo caché por archivo que podía mezclar bloques hermanos.

Each MIR `Block` creates a `DILexicalBlock` nested below the active scope. An RAII guard restores
the parent even when lowering exits on error. Locations and automatic variables consume the active
scope, while parameters and `this` correctly remain at procedure scope. Secondary files create a
`DILexicalBlockFile` below the current scope without reusing the former per-file cache that could
merge sibling blocks.

#### `createGeneratedDebugSubprogram` y `debugSubprogramLocation`

Las fronteras `extern C` reciben subprogramas DI con firma semántica y ubicación de la declaración
original. `callbackGuardThunk` identifica el guard de excepciones del callback, mientras
`defineCAbiWrappers` identifica el wrapper exportado. Ambos usan `Artificial | Thunk`; LLVM los
serializa como `S_THUNK32`, la clasificación estructural estándar que un debugger puede reconocer
sin interpretar prefijos privados.

The `extern C` boundaries receive DI subprograms with the semantic signature and the original
declaration location. `callbackGuardThunk` identifies the callback exception guard, while
`defineCAbiWrappers` identifies the exported wrapper. Both use `Artificial | Thunk`; LLVM
serializes them as `S_THUNK32`, the standard structural classification a debugger can recognize
without parsing private prefixes.

### `tests/integration/debug_pdb/Main.ns`

El fixture `Pair` conserva `Left` y `Right`, y agrega el método de instancia `Total`, que usa
explícitamente `this.Left` y `this.Right`. `Main` incluye un bloque anidado con el local `scoped`,
un callback `Transform` y un export `ExportedIdentity`; el resultado ejecutable continúa siendo 42.

The `Pair` fixture retains `Left` and `Right` and adds instance method `Total`, which explicitly
uses `this.Left` and `this.Right`. `Main` includes a nested block with local `scoped`, a
`Transform` callback and an `ExportedIdentity` export; the executable result remains 42.

### `cmake/VerifyNativeSharpDebugArtifacts.cmake`

El gate exige en ambos niveles de optimización:

- procedimiento `DebugFixture.Pair.Total`;
- `LF_MEMBER Left` público en offset 0;
- `LF_MEMBER Right` público en offset 4;
- `Pair` de 8 bytes;
- `S_LOCAL this` con rango real;
- `S_BLOCK32` y, en O0, el local `scoped` dentro de un bloque hijo;
- un `S_THUNK32` para el callback `Increment__c_boundary`;
- un `S_THUNK32` para el símbolo C `ExportedIdentity`;
- líneas ejecutables actualizadas del fixture.

The gate requires at both optimization levels:

- procedure `DebugFixture.Pair.Total`;
- public `LF_MEMBER Left` at offset 0;
- public `LF_MEMBER Right` at offset 4;
- an 8-byte `Pair`;
- `S_LOCAL this` with a real range;
- `S_BLOCK32` and, at O0, local `scoped` inside a child block;
- an `S_THUNK32` for callback `Increment__c_boundary`;
- an `S_THUNK32` for C symbol `ExportedIdentity`;
- updated executable fixture lines.

## Por qué esta implementación / Why this implementation

Los offsets del depurador deben ser idénticos a los usados por el ejecutable. Por eso el modelo
MIR transporta el resultado autoritativo de layout y DIBuilder sólo lo serializa. El compuesto
reemplazable es la ruta LLVM diseñada para tipos autorreferenciales y evita un formato lateral.

Debugger offsets must match the executable exactly. MIR therefore transports the authoritative
layout result and DIBuilder only serializes it. The replaceable composite is LLVM's designed path
for self-referential types and avoids any sidecar format.

`this` se enlaza al mismo receptor que consume el cuerpo. En O0 el PDB puede expresarlo relativo a
`RCX`; en O2 LLVM puede moverlo, acortar su rango o eliminarlo sin que el compilador publique una
dirección obsoleta.

`this` binds to the same receiver consumed by the body. At O0 the PDB may express it relative to
`RCX`; at O2 LLVM may move it, shorten its range or eliminate it without the compiler publishing a
stale address.

Los scopes se derivan del árbol MIR, no de heurísticas sobre nombres o rangos de instrucciones.
En O2 un local puede convertirse en constante o desaparecer; por eso el gate exige el local
anidado en O0 y conserva en ambos perfiles la estructura que sobrevive a la optimización.

Scopes derive from the MIR tree rather than heuristics over names or instruction ranges. At O2 a
local may become a constant or disappear; the gate therefore requires the nested local at O0 and
preserves at both profiles the structure that survives optimization.

Los wrappers y guards no se disfrazan de métodos del usuario. `S_THUNK32` es más estable que una
etiqueta textual y permite implementar políticas de step-in/step-over usando el tipo de registro.

Wrappers and guards do not masquerade as user methods. `S_THUNK32` is more stable than a textual
label and lets a debugger implement step-in/step-over policy from the record kind.

## Evidencia enfocada / Focused evidence

- compilación incremental de `nsc` desde el contenido actual del disco, limitada a 4 hilos;
- reconstrucción exclusiva de cuatro ejecutables relacionados, sin reconstruir la matriz completa;
- regresión enfocada de lowering, ABI de structs, C ABI, lambdas y PDB: `5/5` en 5.25 s;
- `nsc.debug.codeview-pdb`: verde en O0/O2;
- PDB O0: `LF_FIELDLIST` con `Left @ 0` y `Right @ 4`, ambos `public`;
- PDB O0: `this` como `S_LOCAL` con `S_DEFRANGE_REGISTER_REL`, registro `RCX`;
- PDB O0: bloque hijo `S_BLOCK32` cuyo primer local es `scoped`;
- PDB O0/O2: callback y export serializados como `S_THUNK32`;
- el fixture conserva salida 42.

El baseline completo no se ejecutó durante cada microcambio. Se ejecutó una sola vez al cierre
del lote, después de estas puertas enfocadas:

- corrida: `20260909-010823-1c7394c5`;
- CTest `lld-link`: 307/307;
- fixtures `lld-link`: 148/148;
- bug triage `lld-link`: 18/18;
- CTest `NSLink`: 307/307;
- fixtures `NSLink`: 148/148;
- bug triage `NSLink`: 18/18;
- Gate 0: `LANDED`;
- duración total con `--jobs 4`: 1320.11 s.

The full baseline was not run after each micro-change. It ran once when closing the batch, after
the focused gates above. Run `20260909-010823-1c7394c5` passed 307/307 CTest, 148/148 fixtures and
18/18 triage cases on each linker; Gate 0 landed in 1320.11 s with `--jobs 4`.

## Pendientes declarados / Declared follow-ups

- cobertura de inicializadores estáticos, helpers de destrucción, thunks de interfaz y filtros EH;
- enumeración de variables capturadas y contextos de closures;
- enumeradores completos y otras superficies de tipos NativeSharp;
- checksums de fuente y remapeo reproducible de rutas (`NSC-DEBUG-005`).

## Commit de referencia / Reference commit

**Título ES:** Amplía el PDB con structs, scopes y thunks nativos

**Title EN:** Extend PDB with structs, lexical scopes, and native thunks

**Descripción ES:** Transporta layouts de campos autoritativos a MIR, publica `this` y variables
en scopes léxicos reales, y clasifica wrappers C y guards de callbacks como `S_THUNK32`, con
cobertura dirigida O0/O2.

**Description EN:** Transports authoritative field layouts into MIR, publishes `this` and variables
in real lexical scopes, and classifies C wrappers and callback guards as `S_THUNK32`, with focused
O0/O2 coverage.
