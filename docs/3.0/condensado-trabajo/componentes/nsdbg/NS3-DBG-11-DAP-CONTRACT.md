# Contrato DAP para VS Code / VS Code DAP contract

## Ejecución

La extensión inicia:

```text
nsdbg.exe --dap
```

El canal es `stdin`/`stdout` binario, con un header `Content-Length: N`, línea vacía y exactamente
`N` bytes UTF-8 de JSON. `stderr` queda libre para diagnóstico del adaptador y nunca debe mezclarse
con el protocolo.

La identidad automatizable se consulta con `nsdbg --adapter-info`. `initialize` devuelve además
`nsdbgAdapterVersion` y `nsdbgProtocolVersion`. `sourceFileMap` contiene hasta 64 pares
`prefijo-compilado → prefijo-cliente`; se aplica en sentido inverso a requests y directo a fuentes
salientes.

## Secuencia de lanzamiento

1. Cliente envía `initialize`.
2. Adaptador responde capacidades.
3. Cliente envía `launch` con `program`, `args`, `cwd`, `stopOnEntry` y `enableMemoryWrite` opcionales.
4. El motor crea el proceso, consume eventos internos hasta el breakpoint del loader, responde
   `launch` y emite `initialized`.
5. Cliente configura breakpoints y envía `configurationDone`.
6. Con `stopOnEntry=true`, se emite `stopped(entry)`; en otro caso comienza la ejecución.

Esto mantiene el proceso detenido durante la ventana de configuración y evita perder breakpoints
antes de ejecutar código del usuario.

## Finalización y códigos de salida / Completion and exit codes

Cuando el debuggee termina, `nsdbg` publica primero `exited`, conservando su código en
`exited.body.exitCode`, y después `terminated`. Si el cliente cierra el pipe tras recibir ambos
eventos, `nsdbg.exe --dap` termina con código `0`, aunque el programa depurado haya devuelto un valor
distinto de cero. Un EOF, framing inválido o fallo de escritura antes de completar esa secuencia es
un error del adaptador y conserva código `1`. La extensión no reinterpreta ni oculta ese resultado.

When the debuggee exits, `nsdbg` first publishes `exited`, preserving the target status in
`exited.body.exitCode`, and then publishes `terminated`. Closing the pipe after both events makes
`nsdbg.exe --dap` return `0` regardless of the debuggee status. EOF, invalid framing, or a write
failure before that completed sequence remains an adapter error and returns `1`.

## Requests implementados

| Request | Estado y semántica |
|---|---|
| `initialize` | Anuncia configuración, breakpoints de instrucción/función, módulos, terminate y granularidad. |
| `launch` | Windows x64, proceso hijo, `DEBUG_ONLY_THIS_PROCESS`. |
| `attach` | PID Windows AMD64 existente; no toma ownership de su vida. |
| `configurationDone` | Libera el breakpoint inicial o informa `entry`. |
| `setInstructionBreakpoints` | Reemplazo completo; referencias decimales o `0xHEX`; offset en bytes. |
| `setBreakpoints` | IDs lógicos por documento; cero/múltiples sites, pending y ubicación real desde PDB validado. |
| `breakpointLocations` | Consulta un rango acotado y devuelve líneas/columnas ejecutables deduplicadas del PDB. |
| `setBreakpoints` | Resuelve la línea exacta o la desplaza hacia la siguiente ubicación PDB no generada dentro de 128 líneas; responde con la línea real. |
| `setFunctionBreakpoints` | Nombres exactos, múltiples sites ejecutables, pending y re-resolución por módulos. |
| `setExceptionBreakpoints` | Filtros `all`, `nativesharp` y `uncaught`; second-chance siempre visible. |
| `exceptionInfo` | Código, nombre, chance, continuabilidad y objeto payload para `0xE04E5301`. |
| `threads` | Hilos conocidos por eventos Win32. |
| `modules` | PE x64 cargados: ID de carga, nombre, ruta y rango absoluto ASLR. |
| `stackTrace` | Stack AMD64 físico real; paginación `startFrame`/`levels`, IDs por parada, módulo, símbolo y fuente. |
| `scopes` / `variables` | Arguments, Locals, Captures, Globals y Registers; referencias válidas sólo durante la parada. |
| `evaluate` | Watch/hover/REPL read-only: nombres, campos, índices materializados y operadores puros. |
| `dataBreakpointInfo` / `setDataBreakpoints` | Resuelve valores con dirección, asigna DR0–DR3 y reemplaza sólo los watchpoints propiedad de DAP. |
| `dataBreakpointInfo` / `setDataBreakpoints` | Resuelve valores con dirección, asigna DR0–DR3 y reemplaza sólo los watchpoints propiedad de DAP. |
| `dataBreakpointInfo` / `setDataBreakpoints` | Resuelve valores con dirección, asigna DR0–DR3 y reemplaza sólo los watchpoints propiedad de DAP. |
| `disassemble` | Hasta 256 instrucciones; offsets de bytes/instrucciones, bytes hex, Intel, símbolo/fuente opcional. |
| `readMemory` | Hasta 1 MiB; referencia+offset comprobados, datos base64 y sufijo `unreadableBytes`. |
| `writeMemory` | Base64 acotado; apagado salvo `launch.enableMemoryWrite=true`; parcial sólo si se solicita. |
| `continue` | Continúa todos los hilos del proceso depurado. |
| `stepIn` | Avanza hasta una ubicación fuente distinta; `granularity: instruction` conserva single-step CPU. |
| `next` | Avanza a otra línea sin aceptar un frame físico más profundo. |
| `stepOut` | Avanza hasta un frame físico caller con fuente. |
| `pause` | `DebugBreakProcess`, entregado como `stopped(pause)`. |
| `terminate` | Finalización explícita del debuggee lanzado o adjunto. |
| `disconnect` | En attach, detach por defecto; restaura estado y deja vivo el target. |
| `cancel` | Acknowledge idempotente; no se anuncia cancelación concurrente. |

## Capacidades deliberadamente no anunciadas

- restart;
- edición de variables;
- cancelación concurrente, loadedSources/sourceReference y completions;

Launch y attach publican `progressStart`/`progressEnd` con un `progressId` correlacionado. El
progreso no es cancellable. Cada request exige `seq` positivo, `type=request`, `command` string y
`arguments` object si está presente. El framing limita JSON a 16 MiB y headers a 8 KiB/64 líneas.

## Regla para breakpoints de fuente

NSC y NSLINK producen CodeView/PDB/RSDS verificable. `setBreakpoints` resuelve una línea
ejecutable, instala el site y devuelve la ubicación real. Una línea sin site permanece no
verificada con diagnóstico del debugger. NSLINK es el linker oficial y sus PDB O0/O2 son el gate.

## Eventos de módulo

Después de `initialized`, el adaptador publica un evento `module` con `reason:new` por cada imagen
ya cargada. Cargas posteriores producen `new`; `UNLOAD_DLL` y la salida/terminación del proceso
producen `removed` con el mismo ID. Un ID retirado nunca vuelve a identificar otra carga. El campo
`symbolStatus` informa `loaded`, `missing`, `mismatched` o `failed`; `symbolFilePath` aparece cuando DbgHelp valida y abre el PDB.

## Launch configuration mínima

```json
{
  "type": "nativesharp",
  "request": "launch",
  "name": "NativeSharp: Debug",
  "program": "${workspaceFolder}/bin/program.exe",
  "args": [],
  "cwd": "${workspaceFolder}",
  "stopOnEntry": true,
  "enableMemoryWrite": false
}
```


## Política de memoria / Memory policy

Las lecturas y escrituras sólo se atienden con el target detenido. La dirección efectiva se calcula
sin wraparound, el máximo es 1 MiB y una lectura parcial conserva únicamente el prefijo contiguo.
Las escrituras están deshabilitadas por defecto, prevalidan todo el rango salvo llowPartial=true,
rechazan solapamiento con INT3 propiedad del debugger y vacían la caché de instrucciones.

## Desensamblado / Disassembly

disassemble usa memoryReference más offset, aplica después instructionOffset y devuelve hasta
instructionCount elementos. Los bytes se leen sólo mediante el servicio seguro. Los offsets negativos
buscan una frontera que termina exactamente en la referencia dentro de la región virtual legible; por la
naturaleza variable de x86-64, esa navegación previa es heurística y determinista.

## Ownership de breakpoints / Breakpoint ownership

Los sites DAP usan el dominio Logical, separado de los breakpoints manuales y temporales. Reemplazar
una lista DAP sólo modifica su dominio. Durante restore/single-step/reinsert, nsdbg suspende los demás
hilos; hits ya en cola se rebobinan y serializan. Un cambio del byte original invalida el site en vez
de sobrescribir código automodificado.

## Stepping de fuente / Source stepping

El motor conserva una operación separada para `stepIn`, `next` y `stepOut`. Compara ubicación
archivo/línea y profundidad física obtenida por unwind AMD64; las instrucciones sin línea se
atraviesan como runtime, prólogo o epílogo. Durante la operación sólo corre el hilo seleccionado.
Un breakpoint, pausa o excepción cancela el plan y conserva esa parada de mayor prioridad.

Sin línea o stack inicial fiable, el motor informa el fallback y ejecuta una instrucción. Existe
un presupuesto de 100,000 instrucciones para impedir un loop interno ilimitado. Frames inline
lógicos siguen perteneciendo a `NSD-24`.

## Attach y detach

`attach.processId` debe identificar un proceso Windows AMD64 nativo accesible con los permisos de
la sesión actual. nsdbg no eleva privilegios. Al adjuntar usa `DebugSetProcessKillOnExit(FALSE)`.
Detach sólo se ejecuta detenido: restaura INT3, cancela stepping, revierte suspend counts, continúa
el evento pendiente y reintenta la carrera transitoria de `DebugActiveProcessStop`. Cierra sólo los
handles del debugger. `disconnect` sin `terminateDebuggee` deja vivo el proceso; `terminate` sí lo
termina deliberadamente.

## Estados y generaciones

El motor publica `Inactive`, `Running` o `Stopped` y un `stopGeneration` monotónico por cada parada
visible, incluida salida. DAP conserva frame IDs sólo dentro de esa generación: continue/step los
invalida antes de ejecutar; `scopes` y `variables` rechazan IDs/references viejos. Eventos `stopped` y respuestas
`stackTrace` incluyen `nsdbgStopGeneration` como extensión diagnóstica. La cola del worker sigue
siendo el único hilo propietario de Win32/DbgHelp y shutdown es idempotente.

## Scopes y variables / Scopes and variables

Cada frame físico publica `Arguments`, `Locals`, `Captures`, `Globals` y `Registers`. Las
ubicaciones escalares soportadas son constante, registro AMD64, register-relative y
frame-relative. Un valor cuya ubicación ya no exista aparece como `<optimized out>`; el adaptador
no intenta inferirlo desde memoria cercana. Los IDs quedan ligados a `stopGeneration`. La primera
versión limita cada valor raw a ocho bytes y cada enumeración a 1,024 entradas. Los tipos
compuestos entregan references hijas; la expansión limita 256 hijos, profundidad tres y strings a
4 KiB. Structs, unions, enums, pointers, arrays, clases, herencia, delegates/genéricos ordinarios y
la cabecera string de NSC se interpretan desde CodeView sin ejecutar código del target.

### Frames inline y optimización / Inline frames and optimization

`stackTrace` conserva la identidad del frame físico y añade una entrada lógica por cada contexto
inline que DbgHelp pueda demostrar desde CodeView. Cada entrada incluye extensiones compatibles:

- `nsdbgFrameKind`: `physical` o `inline`;
- `nsdbgFidelity`: `exact`, `optimized` o `unknown`;
- `nsdbgPhysicalFrame`: índice del frame AMD64 que respalda la entrada;
- `nsdbgTailCalls`: `physical-only` o `not-reconstructible`.

Un frame inline usa `presentationHint: subtle` y sus variables se consultan con el contexto inline,
no con una dirección fabricada. Una tail call eliminada no tiene frame físico y DbgHelp no publica
una API para reconstruirla de manera general: NSDBG lo declara como no reconstruible en una vista
optimizada. O0 permanece fiel a los frames físicos; O2 nunca inventa callers, capturas ni valores.

`stackTrace` retains physical-frame identity and adds one logical entry for every inline context
that DbgHelp can prove from CodeView. Each entry carries the compatible extension fields listed
above. Inline variables use the inline scope itself. An eliminated tail call has no physical frame
and DbgHelp exposes no general reconstruction API, so NSDBG marks that limitation explicitly in an
optimized view. O0 remains faithful to physical frames; O2 never invents callers, captures, or
values.

### Async, generadores y módulos / Async, generators, and modules

Un símbolo CodeView validado que termine en `.async#N`, `.generator#N` o
`.async-generator#N` (con `.resume` opcional) conserva su frame físico y se presenta con el nombre
lógico anterior al marcador. DAP añade `nsdbgCoroutineKind`, `nsdbgSuspensionState` y
`nsdbgLogicalFrame`. Un Future o generador suspendido que sólo exista como valor no se inventa como
caller: `variables` muestra su estado y sus campos reales bajo `nsdbgTypeKind`.

Cada objeto de `modules` y cada evento `module` expone `nsdbgImageKind` (`exe`/`dll`) y el
`nsdbgLoadId` monotónico. Cuando la imagen contiene RSDS se publica `nsdbgExpectedPdbIdentity`; sólo
después de que DbgHelp cargó exactamente ese GUID/age aparece `nsdbgLoadedPdbIdentity`. Una DLL
cargada tarde, descargada y vuelta a cargar recibe otra identidad de carga, aunque conserve ruta y
base ASLR.

A validated CodeView symbol ending in `.async#N`, `.generator#N`, or `.async-generator#N` (with an
optional `.resume`) retains its physical backing frame and is displayed with the logical name before
the marker. DAP adds `nsdbgCoroutineKind`, `nsdbgSuspensionState`, and `nsdbgLogicalFrame`. A suspended
Future or generator that only exists as a value is never fabricated as a caller: `variables` shows
its actual state and fields under `nsdbgTypeKind`.

Every `modules` entry and `module` event exposes `nsdbgImageKind` (`exe`/`dll`) and the monotonic
`nsdbgLoadId`. Images with RSDS publish `nsdbgExpectedPdbIdentity`; `nsdbgLoadedPdbIdentity` appears
only after DbgHelp loaded that exact GUID/age. A late DLL unloaded and loaded again receives a new
load identity even if its path and ASLR base are reused.

## Evaluación, condiciones y logpoints

`evaluate` comparte el mismo parser con condiciones y `{expresión}` de logpoints. Acepta
literales, nombres, campos, índices ya materializados, paréntesis, unarios y operadores
aritméticos/bitwise/comparación/booleanos. No acepta asignación ni llamadas. Source/function
breakpoints admiten `condition`, `hitCondition` (`N`, comparadores o `% N`) y `logMessage`. Un
error de condición detiene con diagnóstico; no se oculta. Un logpoint emite output y continúa si
ningún breakpoint hermano solicita parar.

## Data breakpoints AMD64

`dataBreakpointInfo` acepta un valor materializado de `variables` y devuelve un identificador
opaco `0xDIRECCION:tamaño`. `setDataBreakpoints` valida ese identificador, el tipo de acceso y las
reglas físicas de AMD64: máximo cuatro slots, tamaños 1/2/4/8, y alineación natural para datos.
Execute sólo admite un byte. Los registros DR0–DR3/DR6/DR7 se programan en todos los hilos
existentes y en cada hilo creado después; detach restaura el contexto que pertenecía al target.
Una parada reporta `reason: "data breakpoint"` y conserva el ID lógico DAP. El debugger nunca
emula lecturas ni escrituras para aparentar soporte cuando el hardware no puede representarlas.

## Data breakpoints AMD64

`dataBreakpointInfo` acepta un valor materializado de `variables` y devuelve un identificador
opaco `0xDIRECCION:tamaño`. `setDataBreakpoints` valida ese identificador, el tipo de acceso y las
reglas físicas de AMD64: máximo cuatro slots, tamaños 1/2/4/8, y alineación natural para datos.
Execute sólo admite un byte. Los registros DR0–DR3/DR6/DR7 se programan en todos los hilos
existentes y en cada hilo creado después; detach restaura el contexto que pertenecía al target.
Una parada reporta `reason: "data breakpoint"` y conserva el ID lógico DAP. El debugger nunca
emula lecturas ni escrituras para aparentar soporte cuando el hardware no puede representarlas.

## Data breakpoints AMD64

`dataBreakpointInfo` acepta un valor materializado de `variables` y devuelve un identificador
opaco `0xDIRECCION:tamaño`. `setDataBreakpoints` valida ese identificador, el tipo de acceso y las
reglas físicas de AMD64: máximo cuatro slots, tamaños 1/2/4/8, y alineación natural para datos.
Execute sólo admite un byte. Los registros DR0–DR3/DR6/DR7 se programan en todos los hilos
existentes y en cada hilo creado después; detach restaura el contexto que pertenecía al target.
Una parada reporta `reason: "data breakpoint"` y conserva el ID lógico DAP. El debugger nunca
emula lecturas ni escrituras para aparentar soporte cuando el hardware no puede representarlas.

## Excepciones / Exceptions

Las excepciones de programa se continúan con `DBG_EXCEPTION_NOT_HANDLED`, de modo que los handlers
SEH del target conservan su semántica. Breakpoints y single-step internos usan `DBG_CONTINUE`.
`all` detiene first chance; `nativesharp` detiene first chance sólo para `0xE04E5301` y además toda
second chance; `uncaught` deja pasar first chance y detiene second chance. El payload cero del
throw NativeSharp se expone como referencia de objeto sin desreferenciarlo todavía.
