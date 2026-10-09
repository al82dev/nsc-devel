# Host.Runtime — ABI, capacidades y runtime Win32

## 1. Estado y procedencia

- **Clasificación:** `IMPLEMENTACIÓN VIGENTE`, salvo hallazgos marcados.
- **Repositorio:** `A:\nsc-devel`.
- **Revisión congelada:** `3bb175443a18b2049c20d75443dd6743e2993903`.
- **Universo:** 6 archivos, 7,191 líneas y 384,512 bytes.
- **Inventario:** 129 símbolos físicos, 48 contratos y 384 casos diseñados.
- **Validación realizada:** lectura completa de los tres headers y las dos implementaciones pequeñas; índice AST,
  catálogo de exports/imports, firmas, diagnósticos y lectura dirigida completa de `WindowsRuntimeBuilder.cpp`.
- **No realizado:** build, ejecución, baseline, cobertura, instalación, despliegue o modificación del producto.

## 2. Frontera del componente

Host.Runtime materializa el soporte Win32 que acompaña al código NativeSharp y define el contrato binario que
Host.Linker valida. Sus responsabilidades son:

1. publicar el catálogo ABI y su identidad estable;
2. verificar que un módulo LLVM exporte exactamente el subconjunto permitido para su clase de imagen;
3. reportar capacidades runtime listas, parciales o ausentes;
4. construir módulos LLVM de runtime para EXE, biblioteca de soporte, entry point y DLL;
5. adaptar operaciones de consola, entorno, archivos, procesos, threads, sincronización, atomics y EH a Win32.

No decide tipos, ownership ni resolución semántica de llamadas. Tampoco enlaza el producto final: entrega módulos
LLVM que Host.LLVM emite y Host.Linker incorpora a la imagen.

## 3. Contrato ABI congelado

La revisión declara ABI `1.2` y symbol set `4`. `kRuntimeAbiSymbols` contiene 99 entradas: 98 funciones y el dato
TLS `__nativesharp_exception_tls_index` de tipo `i32`. Cada entrada fija nombre, clase, tipo LLVM y presencia por
imagen. El target contractual es `x86_64-pc-windows-msvc`, usa opaque pointers y convención `win64-c`.

`runtimeAbiContractManifest` serializa la identidad y el catálogo en orden canónico. La huella usa FNV-1a de 64
bits y su representación hexadecimal tiene ancho fijo. La escritura y verificación del archivo normalizan el
retorno de carro, pero no toleran contenido faltante, adicional o reordenado.

## 4. Lectura y verificación de IR

`collectActualSymbols` recorre declaraciones y definiciones LLVM, distingue funciones de globals, canonicaliza
tipos de función y rechaza símbolos NativeSharp duplicados. La extracción es deliberadamente dirigida al IR que
produce el compilador; no pretende ser un parser LLVM general.

`verifyRuntimeAbiContract` compara nombres, clases y tipos de forma exacta. La partición observable es:

- `Executable`: catálogo completo;
- `SupportLibrary`: catálogo completo salvo `__nativesharp_start`;
- `EntryPoint`: sólo `__nativesharp_start`.

Un símbolo esperado ausente, uno inesperado, una clase incorrecta o un tipo distinto hacen fallar la verificación.

## 5. Capacidades runtime

El reporte estable `v1` deriva sus estados del catálogo ABI, no de texto documental externo. Conserva cuatro
capacidades:

- `E032` regiones de memoria compartida: `Missing`;
- `E033` sincronización compartida: `Partial`; existen mutex, semáforos, eventos, espera temporizada y monitores,
  pero faltan `SharedMutex`, espera atómica y garantías de fairness;
- `E034` continuaciones de futuros: `Missing`;
- `E035` zonas horarias Windows: `Partial`; sólo existe el hook de `FILETIME`.

`runtimeCapabilityReady` distingue ID desconocido, capacidad no lista y capacidad lista. El reporte incluye target,
ABI, huella y detalle estable por capacidad.

## 6. Forma general del builder LLVM

`WindowsRuntimeBuilder` construye funciones LLVM explícitas. `functionType` acepta hasta 16 parámetros y aborta en
el sitio si una declaración excede el límite, evitando propagar un tipo nulo. Los wrappers conservan tipos Win64,
convierten handles mediante `ptrtoint`/`inttoptr` y usan slots/branches explícitos cuando necesitan estado mutable.

La mayoría de fallos Win32 se proyectan al valor inerte documentado para esa API (`0`, `false`, string vacío o
`-1`). Las operaciones donde eso produciría un dato falso conservan un contrato más estricto.

## 7. Memoria, trap y arranque

El runtime publica allocate/free de heap, trap y filtros SEH. `__nativesharp_start` invoca `Main`, captura el código
de salida y atraviesa la frontera SEH prevista. El filtro de panic reconoce el código reservado del runtime y no
absorbe excepciones ajenas.

## 8. Consola y tiempo

La escritura de consola selecciona stdout o stderr mediante máscara, preserva la longitud NativeSharp y agrega
salto de línea sólo en el modo solicitado. `ReadLine` usa buffer dinámico, diferencia EOF y status y devuelve un
string NativeSharp. `puts` permanece como compatibilidad de C string.

Los clocks cubren contador de alta resolución, frecuencia y system time en formato `FILETIME`; los out-parameters
se leen sólo después de la llamada correspondiente.

## 9. Entorno, strings y argumentos

El entorno expone tick count, número de procesadores, exit, variables, directorio actual y command line. Los
strings NativeSharp se representan como longitud `i64` seguida de bytes. Los límites Win32 convierten a buffers
ANSI terminados en NUL o a UTF-16 según la API; los buffers temporales viven en el stack del wrapper.

Los argumentos usan `CommandLineToArgvW`, convierten cada elemento a UTF-8 y liberan el bloque Win32. El conteo y
la materialización comparten la misma convención de fallo a cero/vacío.

## 10. Archivos y directorios

El runtime implementa existencia diferenciada de archivo/directorio, create/delete, lectura y escritura de texto,
append, tamaño, lectura y escritura de bytes, path absoluto, copy y move. Las capacidades de buffers limitan las
copias; no se escribe ni se lee más allá del arreglo proporcionado.

La enumeración usa `FindFirstFileW`/`FindNextFileW`/`FindClose`, excluye `.` y `..`, filtra archivo o directorio y
separa conteo de llenado. Si el destino se llena, descarta entradas restantes sin rebasarlo.

## 11. Procesos

`Process.Run` crea un proceso con stdout y stderr dirigidos al mismo archivo heredado, espera de forma indefinida y
devuelve su exit code; los fallos de creación o consulta devuelven `-1`. Los wrappers de proceso exponen PID,
working set, tiempos kernel/user y priority class, con setter separado.

## 12. Threads y sincronización

El trampoline de thread despacha delegates estáticos o ligados sin exportarse como símbolo ABI. Create devuelve
handle o cero; join espera y cierra, detach cierra sin esperar, sleep e ID actual completan la familia.

Mutex, semáforos y eventos ofrecen variantes nombradas/no nombradas, operaciones y cierre con guardas para handle
nulo. Los monitores usan SRW lock y condition variable, con enter/exit/pulse/pulse-all/wait. La espera temporizada
comparte la adaptación de handles y produce booleano.

## 13. Sistema y diagnóstico

Los wrappers de sistema publican page size, arquitectura, allocation granularity, memoria física total/disponible
y porcentaje de carga. Diagnóstico cubre debugger presente, conteo de frames capturados y salida al debugger.

## 14. Atomics y helpers de half

Load/store atómicos y las operaciones interlocked de 32/64 bits usan orden `seq_cst`. Increment/decrement, add,
exchange y compare-exchange conservan el valor de retorno esperado por el ABI.

`__truncsfhf2` y `__extendhfsf2` se implementan con manipulación entera IEEE-754. No requieren CRT ni F16C y deben
conservar ceros, subnormales, normales, infinitos y NaN según el algoritmo vigente.

## 15. Excepciones

El runtime inicializa un slot TLS para el payload, permite consultar y limpiar la excepción actual y lanza mediante
SEH con código `0xE04E5301`. El filtro tipado reconoce ese código, importa payloads cruzando módulos y deja pasar
excepciones ajenas. Panic usa el código reservado `0xE04E5302`.

## 16. Modos de construcción

Las rutas públicas producen:

- `build()`: imagen ejecutable con símbolo `Main` predeterminado;
- `build(mainSymbol)`: ejecutable con main explícito;
- `buildSupport()`: soporte sin entry point;
- `buildEntry(mainSymbol)`: módulo que contiene sólo el arranque;
- `buildDynamicLibraryAnchor()`: anchor mínimo de DLL;
- `buildInternal(...)`: ensamblaje común del runtime.

Cada ruta termina verificando el módulo LLVM y, cuando aplica, el contrato ABI de la clase de imagen. El anchor
`__nsc_dll_anchor` y `_fltused` son auxiliares de producto, no exports NativeSharp adicionales.

## 17. Dependencias

- `Host.LLVM`: ownership de módulo, verificación LLVM y posterior emisión.
- `Host.Target`: target y ABI x64 Windows.
- `Host.Linker`: consume y verifica el runtime construido.
- LLVM-C: creación de tipos, funciones, bloques e instrucciones.
- Win32: Kernel32, User32, Psapi, Shell32 y primitivas SEH/sincronización.

## 18. Invariantes observables

1. El catálogo ABI tiene orden, versión, nombre, clase y tipo estables.
2. Cada clase de imagen contiene exactamente su subconjunto permitido.
3. No se publica un módulo que falle verificación LLVM o ABI.
4. Buffers, handles y out-parameters no se usan fuera de sus límites o antes de inicializarse.
5. Un fallo Win32 se convierte sólo al sentinel definido por la operación.
6. Los atomics conservan tamaño y orden `seq_cst`.
7. El estado EH es TLS y los filtros no capturan códigos ajenos.
8. Las capacidades reportan lo implementado; no convierten evidencia parcial en `Ready`.

## 19. Riesgos y hallazgos conservados

1. `WindowsRuntimeBuilder.cpp` concentra 6,543 líneas y 105 funciones; es una frontera de mantenimiento grande.
2. Modularizarlo podría acelerar compilaciones incrementales y permitir paralelismo de build, pero no cambia por sí
   sola el tiempo que NSC tarda en analizar programas.
3. Las pruebas se diseñan contra contratos públicos/observables para sobrevivir una futura modularización.
4. La lectura de IR del contrato ABI es dirigida y depende de la forma textual LLVM vigente.
5. Varias APIs colapsan fallos Win32 diferentes al mismo sentinel; ése es el contrato vigente, no una mejora nueva.
6. El límite de 16 parámetros de `functionType` es deliberadamente ruidoso y no recuperable.
7. `Process.Run` combina stdout y stderr y espera sin timeout.

No se corrigió ninguno de estos puntos en esta fase.

## 20. Estrategia futura de pruebas

El ABI usará IR mínimo generado por fixtures y mutaciones de símbolo/tipo/imagen. Las capacidades usarán el catálogo
como entrada controlada. Los wrappers se validarán con inspección LLVM y, en una capa separada, con programas `.ns`
mínimos sobre Win32. Los oráculos comprobarán firmas, CFG, imports, sentinels y efectos, no nombres de helpers
internos ni el número de `if` del builder.

Cada contrato recibe nominal mínimo, representativo, prerequisito ausente, estructura inválida, límites, variantes,
determinismo y escala. La última matriz es `NIGHTLY`.

## 21. Estado de cierre

Los 6 archivos y 129 símbolos están trazados a 48 contratos con 384 casos diseñados. `Host.Runtime` queda
`AUDITADO` y `RECONSTRUIBLE` para la revisión congelada. Esto no afirma ejecución ni cobertura.
