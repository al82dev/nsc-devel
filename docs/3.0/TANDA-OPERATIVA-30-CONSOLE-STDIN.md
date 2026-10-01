# Tanda operativa 30 — Console, entrada estándar y redirección

Fecha: 2026-09-30

Rama: `development`

Estado: implementación terminada y validación dirigida O0 correcta; baseline no ejecutado y
reservado para la ejecución externa de Alberto

## Objetivo

Cerrar la carencia de entrada estándar secuencial y la superficie mínima de consola sin confundir
dos contratos distintos:

1. `Console.ReadLine` consume stdin y funciona con consola, archivo o pipe;
2. `ConsoleUtils.GetCh` conserva su semántica interactiva de tecla y no participa en lectura
   secuencial;
3. `Console.Write`, `WriteLine`, `ErrorWrite` y `ErrorWriteLine` aceptan tipos primitivos;
4. las cuatro operaciones ofrecen overload sin argumentos; `Write` es una escritura vacía y
   `WriteLine` emite únicamente el terminador de línea;
5. EOF, línea vacía, error y buffer insuficiente son estados observables diferentes.

## Superficie pública

`NativeSharp.Core.Console` incorpora:

```nativesharp
owned string ReadLine(out int status);
owned string ReadLine();
int ReadLine(char[] destination);
```

El overload con estado es el contrato fundamental:

| Resultado | Estado | Significado |
|---|---:|---|
| string no nulo, incluso longitud cero | `0` | se leyó una línea |
| `null` | `-1` | EOF antes de recibir bytes |
| `null` | `-3` | error de I/O o de asignación |
| conteo en buffer | `>= 0` | caracteres copiados |
| resultado del overload de buffer | `-2` | el destino es insuficiente |

El overload de buffer consume la línea completa antes de informar `-2`; una siguiente lectura
empieza en la línea siguiente. CRLF pierde el `CR` final y LF termina la línea directamente. Una
línea vacía devuelve una string válida de longitud cero, nunca el mismo `null` usado por EOF.

La unidad almacenada es el byte UTF-8 que ya usa el layout de `string`. Archivo y pipe conservan
los bytes Unicode sin recodificación. La consola real entra por `STD_INPUT_HANDLE` y por la misma
ruta Win32 `ReadFile`; por tanto, su code page debe producir UTF-8, que es el contrato de texto de
NativeSharp en Windows x64.

## Salida primitiva

Las variantes stdout y stderr reutilizan `String.ToString`, por lo que no nace un segundo formato
numérico. Se agregaron overloads para `bool`, `char`, `int`, `uint`, `long`, `ulong`, `float` y
`double`. La ampliación de `nint`/`nuint`, pointers y formatting tipado continúa en T45/T47, donde
se resolverán las ambigüedades de conversión ya inventariadas; T30 no oculta esa deuda.

## Cadena del compilador

La firma exacta `NativeSharp.Core.Console.ReadLine(out int)` se reconoce como operación core. La
representación interna de un parámetro `out int` es `builtin:int*`; reconocer el pointee en vez del
tipo escrito evitó que la llamada se degradara silenciosamente al cuerpo placeholder de la
biblioteca.

El nodo recorre MethodBody -> HIR -> MIR -> LLVM y termina en:

```text
ptr __nativesharp_console_readline(ptr status)
```

El argumento `out` se baja una sola vez mediante el `AddressValue` que ya crea la ruta ordinaria
de argumentos by-reference. No se introduce una segunda convención de llamadas.

## Runtime Windows x64

El runtime solicita `STD_INPUT_HANDLE` y lee mediante `ReadFile`, que funciona con handles de
consola, archivo y pipe. No llama a `GetCh` ni a `ReadConsole`, y no mantiene estado duplicado.

La string empieza con capacidad de 256 bytes y crece con `HeapReAlloc`. El buffer mantiene el
layout nativo `[longitud i64][bytes UTF-8]`; se libera en EOF vacío o error. `ERROR_BROKEN_PIPE`
equivale a EOF y cualquier otro error produce estado `-3`.

La nueva frontera binaria se publicó de forma explícita:

- runtime ABI `1.1`;
- symbol set `3`;
- 98 símbolos;
- huella `30735111cdb1921f`;
- símbolo nuevo `__nativesharp_console_readline ptr(ptr)`;
- imports nuevos `HeapReAlloc` y `GetLastError` desde `KERNEL32.dll`.

## Archivos modificados

- `lib/NativeSharp.Core/Console.ns`: overloads sin argumentos, overloads primitivos y las tres
  variantes de `ReadLine`.
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h` y
  `src/Frontend/ProjectMethodBodyModel.cpp`: operación core y reconocimiento exacto de
  `owned string ReadLine(out int)` usando `pointeeTypeKey`.
- `include/nativesharp/Frontend/ProjectOopHirModel.h` y
  `src/Frontend/ProjectOopHirModel.cpp`: nodo HIR y lowering del argumento `out`.
- `include/nativesharp/Frontend/ProjectOopMirModel.h` y
  `src/Frontend/ProjectOopMirModel.cpp`: nodo MIR y traducción HIR/MIR.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`: declaración y llamada al hook de runtime.
- `src/Runtime/WindowsRuntimeBuilder.cpp`: lector secuencial, crecimiento, EOF, CRLF/LF y error.
- `src/Linker/WindowsRuntimeImports.cpp`: imports de `HeapReAlloc` y `GetLastError`.
- `include/nativesharp/Runtime/RuntimeAbiContract.h`: ABI 1.1, symbol set 3 y símbolo nuevo.
- `tests/unit/Item400RuntimeAbiContractTests.cpp`: evidencia exacta de versión, conteo, símbolo y
  huella.
- `CMakeLists.txt`: registro de la prueba dirigida T30.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T30ConsoleInputTests.cpp`: fixture O0 aislado que usa el compilador y el
  runtime 3.0 generados en `A:\nsc-devel`; comprueba archivo, pipe, CRLF, LF, línea vacía, UTF-8,
  EOF, buffer insuficiente, stdout, stderr, overloads sin argumentos, superficie primitiva y
  presencia del hook LLVM. La biblioteca mínima del fixture evita arrastrar defectos pendientes de
  otros archivos de Core y nunca consulta `C:\native-sharp`.
- `docs/3.0/TANDA-OPERATIVA-30-CONSOLE-STDIN.md`: expediente de decisiones, ABI, archivos,
  mejora, límites y validación de este commit.

## Validación dirigida ejecutada

1. `nsc` construido en `A:\nsc-devel\build-lsp-debug`, Debug/O0: correcto.
2. `nsc_ns3_t30_console_input_tests`: `PASS: 8 checks`.
3. `nsc_stage41_arrays_strings_core_tests`: `PASS: 76 checks`.
4. `nsc_windows_runtime_tests`: `PASS: 34 checks`.
5. `nsc_item400_runtime_abi_contract_tests`: `PASS: 16 checks`.
6. `nsc_item401_independent_windows_runtime_tests`: `PASS: 22 checks`.
7. `git diff --check`: correcto antes del commit.

No se ejecutó baseline, CTest general, O1/O2/O3, libraries, instalación, deploy, componentes
externos ni operaciones remotas.

## Mejora concreta

Un programa puede leer líneas desde una terminal o desde automatización sin fingir argumentos de
línea de comandos y sin usar una primitiva de teclado incompatible con redirección. Los ejemplos
introductorios imprimen valores sin conversiones manuales, mientras stdout/stderr conservan una
única representación canónica.

## Límites deliberados

- `GetCh` no cambia y sigue siendo exclusivamente interactivo.
- T30 no entrega todavía `Console.In`/`TextReader`, lectura tokenizada ni parsing numérico.
- El overload de buffer informa insuficiencia después de consumir la línea; no conserva un resto
  oculto para otra llamada.
- La entrada de texto es UTF-8. Una consola configurada con otra code page debe cambiarse a UTF-8;
  archivo y pipe no dependen de la code page.
- Sincronización/atomicidad de escrituras concurrentes pertenece a T48.
- Pointers, `nint`/`nuint` y formatting ampliado permanecen en T45/T47.
- El baseline integral no forma parte de esta tanda y será ejecutado externamente por Alberto.
