# Tanda 50 — SH48: biblioteca y runtime completos

| Campo | Valor |
|---|---|
| Estado | `LANDED` — gate dirigido y baseline acumulado aprobados |
| Base | T49/SH47 `af112ee58` |
| Gate | SH48 |
| Instalación | `A:\native-sharp` |
| LLVM | `C:\clang-llvm` (sólo lectura) |

## Objetivo

Reconstruir con el compilador self-hosted las 80 unidades de Core, Collections, IO, Diagnostics y
Threading que forman la biblioteca 2.0, producir bibliotecas COFF deterministas en O0–O3 y enlazar
un programa real contra el runtime Win32 con LLD y NSLink. El gate debe rechazar entradas de
ejecutable, dependencias ocultas del compilador semilla y superficies ABI incompletas.

## Diseño e implementación

El driver incorpora `--emit-project-library-object`: recorre el grafo fuente completo, incluye sus
dependencias, marca las funciones de biblioteca y emite un único objeto COFF sin exigir `Main`.
Los métodos genéricos abiertos permanecen excluidos hasta su especialización; los accesores
sintetizados reciben nombres estables para sus parámetros.

El backend completa las formas que la biblioteca real descubrió: `sbyte`, `default(T)`, celdas y
llamadas de delegates administrados, referencias crudas de función en conversiones contextuales a
`extern C delegate`, selección exacta de intrínsecos de texto y validación LLVM por función. Esa
conversión contextual es importante: un callback C no puede arrastrar el asignador de delegates
del runtime ni duplicar `__nativesharp_eh_init`.

Los hooks de NativeClock, Environment, File, Directory, Path, Process, SystemInfo, Debug, Thread,
Mutex, Semaphore y Event bajan a la ABI Win32 publicada. `File.Exists` y `Directory.Exists`
conservan una implementación combinada mientras el grafo self-hosted compile ambos tipos en la
misma unidad de biblioteca.

El verificador produce objetos y archivos `.lib` para O0–O3, comprueba identidad al repetir cada
emisión, audita la ausencia de símbolos de entrada de ejecutable, verifica los grupos ABI del
runtime y ejecuta ocho binarios de humo: cuatro perfiles por LLD y NSLink.

## Archivos, métodos y funciones

| Archivo | Responsabilidad |
|---|---|
| `selfhost/nsc/Driver/Program.ns` | comando `--emit-project-library-object` y emisión con dependencias |
| `selfhost/nsc/CodeGen/LlvmBackend.ns` | modo biblioteca, delegates, hooks runtime y verificación LLVM |
| `lib/NativeSharp.LLVM.C/LLVMApi.ns` | binding auditado de `LLVMVerifyFunction` |
| `selfhost/tests/runtime-2.0/RuntimeLibraries.nsproj` | grafo de las 80 unidades de biblioteca |
| `selfhost/tests/runtime-2.0/RuntimeBundle.ns` | raíz sin punto de entrada para el archivo COFF |
| `selfhost/tests/runtime-2.0/RuntimeSmoke.nsproj` | consumidor enlazable por ambos linkers |
| `selfhost/tests/runtime-2.0/RuntimeSmoke.ns` | ejercicio observable de biblioteca y runtime |
| `selfhost/contracts/runtime-library.contract.json` | inventario, perfiles, ABI y criterios del gate |
| `selfhost/bootstrap/verify_runtime_library.py` | emisión, repetición, archivo, auditoría, enlace y ejecución |
| `cmake/VerifyNativeSharpRuntimeLibrary.cmake` | integración reproducible del gate SH48 |

Métodos principales: `EmitProjectBackend`, `MarkLibraryFunctions`, `ExternDelegateType`,
`RawFunctionReference`, `EmitManagedDelegateValue`, `EmitManagedDelegateCall`, `RuntimeHookName`,
`EmitRuntimeHook`, `IntegerBits`, `EmitDefaultValue` y `EmitInlineAssembly`.

## Incidencias reales cerradas

El primer baseline descubrió que SH46 materializaba callbacks C mediante la ruta de delegate
administrado, incorporando una segunda dependencia del runtime. Se separó la representación
contextual de `extern C delegate` y se repitieron SH41, SH46 y SH48.

La siguiente repetición activó el nuevo verificador por función y encontró que el ensamblador
inline de SH47 construía su firma LLVM desde un tipo aproximado. Ahora usa el tipo LLVM efectivo
del local de la función. SH47 volvió a aprobar sus ocho ejecuciones y el baseline completo pasó.

## Evidencia dirigida

- 51/51 pruebas de infraestructura;
- 80 archivos fuente incluidos, sin objetos ni ejecutables como entradas;
- objetos y bibliotecas deterministas en O0–O3;
- cuatro grupos ABI del runtime verificados;
- 8/8 ejecuciones de humo: O0–O3 con LLD y NSLink;
- SH41 real aprobado después de los cambios;
- SH46, SH47 y SH48 aprobados después de las correcciones.

SHA-256 del contrato: `567a1108a49caadf4616cbe1fd1097735d9ec1ac19984c9c09007be171292f65`.
SHA-256 del verificador: `5d0c65a9f856d36e04fdbf73b4879f132499dc71ef057d62f023f12a3dade35f`.
SHA-256 del reporte dirigido: `a9d50083c9076731bf762b4218172cfc7f510d8f7de811b69f2884d8b6a181d2`.

## Baseline acumulado

Corrida congelada `tanda-50-sh48-runtime-library-r4-20260921`, identidad
`5000000000000004`, aprobada en 7832.537 s y sin fallos:

- 483/483 CTests con LLD en 3859.937 s;
- 148/148 fixtures y 18/18 casos de triaje con LLD;
- 483/483 CTests con NSLink en 3818.887 s;
- 148/148 fixtures y 18/18 casos de triaje con NSLink;
- instalación verificada exclusivamente en `A:\native-sharp`;
- Gate 0 certificado en estado `LANDED`.

SHA-256 del reporte del baseline:
`8346586ad620f64fa29ac5956eccd1e8f1f3bccce19ca3552e91e0c4b2c4d17a`.

## Criterios de cierre

- [x] biblioteca completa reconstruida desde sus 80 fuentes;
- [x] objetos y archivos COFF deterministas en O0–O3;
- [x] biblioteca sin `Main`, CRT de ejecutable ni entradas ocultas;
- [x] runtime Win32 y sus cuatro grupos ABI auditados;
- [x] programa consumidor ejecutado con LLD y NSLink;
- [x] punto fijo SH41 posterior a los cambios;
- [x] baseline completo acumulado y Gate 0;
- [x] commit único de T50.
