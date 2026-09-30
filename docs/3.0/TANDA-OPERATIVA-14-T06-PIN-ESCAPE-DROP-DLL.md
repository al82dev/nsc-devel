# Tanda operativa 14 — T06 pin, escape, drop glue y frontera DLL

## Objetivo

Conectar una vertical productiva y conservadora de T06-F06/F08 al compilador real Windows x64:
un owner edition 3 puede fijarse con `pin raii`, permanecer prestable pero no relocatable, conservar
cleanup léxico y ser destruido exactamente una vez incluso cuando su implementación y allocator
pertenecen a una DLL precompilada.

T06-F07 y el drop glue genérico completo no se simulan. El análisis local de escape/ref/VLA ya
existente sigue activo, pero `escape-v1` continúa como helper aislado porque todavía no existe un
summary público derivado del cuerpo, versionado en NSMETA y consumido por calls/closures/threads.

## Auditoría previa

- `OwnershipSemantics` tenía una regla de operaciones permitidas sobre pinned state, pero parser,
  method-body, HIR/MIR y fuente NativeSharp no la consumían.
- El compilador productivo ya destruye locals RAII y parámetros owned leyendo el destruction entry
  del descriptor real del objeto; un consumidor no recompone el destructor privado.
- NSMETA ya publica `destructionEntryName`, `descriptorAccessorName` y ownership de firmas/campos.
- Una clase local sobre base precompilada ya delega terminalmente el tramo externo al productor.
- Faltaba una sintaxis pin productiva y el verificador aceptaba un dispatch contract actual con
  `destructionEntryName` vacío, dejando que el fallo apareciera tarde al materializar/ligar.

## Diseño implementado

### `pin raii T`

`pin` es contextual: sólo se interpreta como feature cuando precede inmediatamente a `raii`, por
lo que no roba el identificador ordinario `pin`. Está disponible únicamente en `<Language>3`.

Un local `pin raii T owner = ...`:

- usa exactamente la inicialización, ownership, cleanup y lowering de un local RAII existente;
- puede originar borrowed locals;
- publica `pinned=true` en el method-body dump;
- no puede ser fuente de `move(owner)`;
- no puede ser reemplazado por `owner = move(other)`;
- no permite partial move de una proyección hasta que exista el modelo pinned-field;
- se destruye en su storage original al salir del scope.

No se introdujo wrapper runtime, allocation, flag dinámico ni ABI nuevo. La inmovilidad es una
obligación estática. Esta vertical fija el owner completo; proyección pinned, self-reference
in-place y coroutine frames pertenecen a la expansión posterior de F06.

### Drop cross-module fail-closed

El formato NSMETA actual ya contiene el destruction entry. No se incrementó schema ni se duplicó
el descriptor. `verifyCurrentCompiler` ahora rechaza un dispatch contract de formato moderno que
no publique `destructionEntryName`, con un diagnóstico de producer-owned destruction entry.

El witness DLL demuestra el camino físico:

1. productor edition 3 emite `.dll`, import `.lib` y `.nsmeta`;
2. consumidor materializa la API pública desde LibraryReference;
3. construye un `Payload` del productor dentro de un local `pin raii`;
4. el scope del consumidor lee el descriptor y llama el drop entry de la DLL;
5. el destructor incrementa estado estático propiedad del productor;
6. consumidor observa exactamente una destrucción y termina en 42.

## Cambios por archivo

### Modificados

- `src/Parser/Parser.cpp`
  - reconoce contextualmente `pin raii` en declaraciones locales;
  - exige edition 3 y conserva el marcador semántico en el AST.
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
  - añade `MethodLocalSymbol::isPinned` como estado productivo del local.
- `src/Frontend/ProjectMethodBodyModel.cpp`
  - liga el marcador pinned;
  - rechaza move-out y replacement del owner con diagnósticos causales.
- `src/Frontend/ProjectMethodBodyModelDumper.cpp`
  - publica `pinned=true|false` junto con RAII y ownership.
- `src/Interop/LibraryMetadata.cpp`
  - falla cerrado si una clase de NSMETA moderno carece del destruction entry del productor.
- `tests/unit/Cc1CorrectnessTests.cpp`
  - registra c64 y cuatro rechazos permanentes para edition/move/replace/projection.
- `tests/unit/Stage50_9MetadataHostCompatibilityTests.cpp`
  - corrompe el drop contract en memoria y exige el rechazo exacto del verificador.
- `docs/3.0/TANDA-06-OWNERSHIP-PRESTAMOS-Y-DESTRUCCION.md`
  - registra el alcance productivo y la deuda aún abierta de F06–F08.

### Nuevos

- `tests/cc1/c64_ns3_pin_drop_boundary/c64_ns3_pin_drop_boundary.nsproj`
  - fixture edition 3 mínima.
- `tests/cc1/c64_ns3_pin_drop_boundary/Main.ns`
  - pin, borrow, lectura y destructor exactamente una vez; devuelve 42.
- `docs/3.0/TANDA-OPERATIVA-14-T06-PIN-ESCAPE-DROP-DLL.md`
  - diseño, evidencia, archivos, mejora y límites del commit.

No se creó ni modificó código fuente en `C:`. Todos los compiladores, proyectos, DLLs, metadata,
ejecutables y temporales usados fueron de `A:`.

## Mejora concreta

- El lenguaje expresa por primera vez una obligación de inmovilidad en fuente productiva.
- Un owner pinned puede prestarse sin que después sea posible relocalizarlo por move o replacement.
- Cleanup no bifurca: pinned reutiliza el mismo drop LIFO probado por RAII.
- Metadata rota ya no llega hasta un error opaco de enlace/destrucción; se rechaza en la frontera.
- El witness demuestra que el consumidor ejecuta el destructor privado del productor sin duplicar
  allocator, cuerpo o ownership.

## Validación dirigida ejecutada

1. Target `nsc` Debug/O0 construido correctamente en `A:\nsc-devel\build-clang`.
2. c64 emitido con el `nsc.exe` recién construido en O0: proceso termina en 42.
3. Method-body dump: `stable` conserva `raii=true pinned=true`, borrow y cleanup region.
4. Cuatro negativos individuales:
   - edition 2: NS3002 `pin raii locals requires <Language>3</Language>`;
   - move de owner pinned: exit 1 con `cannot move pinned owner 'stable'`;
   - replacement: exit 1 con `cannot replace pinned owner 'stable'`.
   - partial move: exit 1 con `cannot partially move from pinned owner 'stable'`.
5. Witness DLL O0:
   - productor `.dll/.lib/.nsmeta` correcto;
   - consumidor reporta `linked-libraries: 1` e importa `PinDropProducer.dll`;
   - ejecución junto a la DLL termina en 42 después de observar un solo destructor del productor.
6. La carga del LibraryReference ejercita la validación positiva del drop contract; el negativo
   permanente se añadió al test específico de compatibilidad NSMETA para el baseline final.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación, deploy ni operación remota.

## Deuda explícita

- Pin de campos/proyecciones, construcción stack/in-place autoreferencial, async y FFI.
- Escape summaries derivados de cuerpos, cadena causal, NSMETA e invalidación incremental.
- Lifetimes named/outlives y asociación exacta de borrowed return con receiver/parámetro.
- Drop glue genérico para arrays, records, tuples y variants owning/move-only.
- Matriz DLL con ambos linkers y allocator personalizado; la vertical actual usa el descriptor ABI
  productivo y un witness O0.
- Debugger/LSP de pinned/borrow graph.

La tanda operativa 14 queda cerrada como vertical F06 + frontera F08. El bloque maestro T06 sigue
`IMPLEMENTING`; no se declara baseline ni gate T06 completo.
