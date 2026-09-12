# 220 — Corpus de depuración extrema O0 / Extreme O0 debug corpus

## Resultado / Outcome

**ES.** NativeSharp incorpora un producto de aceptación O0 separado en productor y consumidor. El
productor se publica como biblioteca estática precompilada con NSMETA; el consumidor no comparte
sus fuentes, enlaza el archive, ejecuta con resultado 42 y produce un PDB final que debe contener
simultáneamente la superficie CodeView de ambos módulos.

**EN.** NativeSharp now carries an O0 acceptance product split into producer and consumer. The
producer is published as a precompiled static library plus NSMETA; the consumer shares no producer
source, links the archive, exits with 42, and emits a final PDB that must simultaneously contain the
CodeView surface from both modules.

## Por qué existe / Why it exists

El fixture histórico demuestra muchas construcciones en un solo EXE. Eso valida lowering y PDB,
pero no demuestra el caso que necesita un debugger sobre frameworks reales: una función que vive
en un `.lib`, se selecciona al enlazar otro proyecto y conserva su módulo, archivo, tipos, locales y
rangos dentro del PDB del consumidor.

The historical fixture proves many constructs in one EXE. That validates lowering and PDB output,
but it does not prove the framework scenario a debugger needs: a function lives in a `.lib`, is
selected while linking another project, and retains its module, source, types, locals, and ranges
inside the consumer PDB.

## Escenario / Scenario

### Productor precompilado / Precompiled producer

`tests/integration/debug_pdb_extreme/Producer.ns` publica:

- `Transform`, delegate invocable entre módulos;
- `ICounter`, contrato de interfaz;
- `Counter`, clase con constructor, implementación de interfaz, campo privado y destructor;
- `Pair`, struct con miembros y método de instancia que usa `this`;
- `Box<T>` y la especialización pública `Box<int>`;
- `ProducerApi.Invoke`, `MakePair`, `ReadInterface` y `ReadBox`.

The producer publishes a cross-module delegate, an interface contract, a concrete class with
constructor/destructor and private state, an instance struct, a generic class and closed
specialization, plus API methods crossing delegate, aggregate, interface, and generic boundaries.

### Consumidor / Consumer

`tests/integration/debug_pdb_extreme/Consumer.ns` usa exclusivamente `LibraryReference` para:

1. convertir un método precompilado a delegate;
2. invocarlo a través de la API productora;
3. recibir y ejecutar un struct por valor;
4. construir una clase y despacharla como interfaz;
5. construir/leer `Box<int>` entre módulos;
6. destruir explícitamente los objetos;
7. devolver 42.

The consumer uses only `LibraryReference` to bind a precompiled method as a delegate, cross the
producer call boundary, receive and invoke an aggregate, dispatch a class through an interface,
construct/read `Box<int>` across modules, destroy owned objects, and return 42.

## Gate automatizado / Automated gate

`cmake/VerifyNativeSharpExtremeDebugO0.cmake` ejecuta una sola configuración O0:

1. copia las dos fuentes a un directorio de producto aislado;
2. escribe proyectos productor/consumidor sin ProjectReference;
3. compila el productor con `--emit-project-oop-lib -g -O0`;
4. exige `.lib` y `.nsmeta`;
5. inspecciona el archive con `llvm-readobj --sections --codeview`;
6. exige `.debug$S`, `.debug$T`, checksum y el procedimiento del productor;
7. compila el consumidor con `--emit-project-oop-exe -g -O0`;
8. exige EXE y PDB y ejecuta el EXE con salida 42;
9. valida RSDS basename-only;
10. inspecciona el PDB completo y exige símbolos, tipos, locales, rangos y fuentes de ambos módulos.

The CMake gate performs the same ten steps in one O0 lane. It deliberately does not add O2 because
this delivery is the stable, high-fidelity debugger handoff; optimized-value honesty remains
covered by the existing CodeView/PDB gate.

## Evidencia exigida / Required evidence

El gate busca de forma explícita:

- `DebugExtreme.ProducerApi.Invoke` procedente del archive;
- `DebugExtremeConsumer.Program.Main` procedente del objeto consumidor;
- `DebugExtreme.Counter.Read` para dispatch de interfaz;
- `DebugExtreme.Pair.Sum` y `DebugExtreme.Box<int>` para tipos compuestos/genéricos;
- locales `seed`, `interfaceView` y `answer`;
- `Producer.ns` y `Consumer.ns`;
- al menos un `S_DEFRANGE`;
- RSDS que nombre `DebugExtremeConsumer.pdb` sin ruta del checkout.

The gate explicitly requires the producer and consumer procedures, interface-dispatch target,
aggregate and generic types, representative locals, both source files, a location range, and a
portable RSDS name.

## Archivos / Files

| Archivo / File | Propósito / Purpose |
|---|---|
| `tests/integration/debug_pdb_extreme/Producer.ns` | Superficie de biblioteca depurable. |
| `tests/integration/debug_pdb_extreme/Consumer.ns` | Consumidor fuente-cero del productor. |
| `cmake/VerifyNativeSharpExtremeDebugO0.cmake` | Construcción, ejecución e inspección estándar. |
| `CMakeLists.txt` | Registra `nsc.debug.extreme-o0`. |

## Decisiones / Decisions

- **O0 es normativo aquí.** El fixture prioriza inspección exacta y baja latencia.
- **Separación física real.** No se usa ProjectReference ni se compilan fuentes del productor en el
  consumidor.
- **Herramientas estándar.** La evidencia se obtiene con `llvm-readobj` y `llvm-pdbutil`; no existe
  sidecar NativeSharp.
- **Ejecución obligatoria.** Un PDB estructuralmente atractivo no sirve si el ABI del programa
  cambió.
- **Debugger de sólo lectura.** El artefacto queda listo para consumo externo; este commit no toca
  el repositorio del debugger.

- **O0 is normative here.** The fixture favors exact inspection and low latency.
- **Real physical separation.** No ProjectReference or producer source enters the consumer.
- **Standard tools.** Evidence comes from `llvm-readobj` and `llvm-pdbutil`, not a NativeSharp
  sidecar.
- **Execution is mandatory.** Structurally plausible debug data is insufficient if program ABI
  behavior changed.
- **Read-only debugger boundary.** The artifact is ready for an external consumer; this commit does
  not modify the debugger repository.

## Verificación durante iteración / Iteration verification

El gate se registra para el cierre acumulativo, pero no se ejecuta durante esta entrega. Como en
las demás entregas del lote, inmediatamente después del commit sólo se reconstruye desde cero el
objetivo `nsc`. El baseline completo se reserva para después de 234.

The gate is registered for cumulative closure but is not executed during this delivery. As with
the other batch deliveries, only a clean `nsc` rebuild follows the commit. The complete baseline is
reserved until after 234.

## Commit de referencia / Reference commit

**Título ES:** Certifica la superficie de depuración O0

**Title EN:** Certify the O0 debug surface

**Descripción ES:** Agrega un corpus productor/consumidor precompilado que conserva CodeView desde
una biblioteca estática hasta el PDB final y certifica tipos, delegates, interfaces, genéricos,
destrucción, locales, fuentes, rangos, RSDS y ejecución 42.

**Description EN:** Add a precompiled producer/consumer corpus that preserves CodeView from a
static library into the final PDB and certifies types, delegates, interfaces, generics,
destruction, locals, sources, ranges, RSDS, and exit code 42.
