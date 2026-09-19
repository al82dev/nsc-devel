# NativeSharp 2.0 self-hosting — tanda 04: fronteras LLVM, runtime y plataforma

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Tareas | `NS2-SH-030` a `NS2-SH-044` |
| Gate | `SH03` |
| Rama | `development` |
| Milestone | `nativesharp-2.0-selfhost-milestone` (`6a9b94850`) |
| Commit de entrada | `1385b84d0` |
| Commit de cierre | este commit (`selfhost(tanda-04): freeze LLVM and platform boundaries`) |
| Fecha de apertura | 2026-09-19 |
| Fecha de cierre | 2026-09-19 |
| Baseline de cierre | `tanda-04-sh03-final-20260919` |

## 2. Objetivo y alcance

### Objetivo

Eliminar dependencias C++ ocultas de la próxima implementación del compilador: una aplicación
NativeSharp debe usar LLVM-C 23 directamente, emitir un objeto COFF, enlazarlo mediante adaptadores
NativeSharp con `lld-link` y NSLink, verificar ambos PE y ejecutarlos con el resultado esperado.
Runtime y plataforma quedan detrás de contratos versionados antes de portar el frontend.

### Incluido

- inventario exacto de 219 imports LLVM-C del `nsc0` actual;
- binding bootstrap de 42 funciones, handles opacos, cinco enums ABI y política LLVM 23.x;
- contexto, módulo, tipos, builder, verifier, target machine, data layout, passes, debug metadata y
  object emission ejercitados desde NativeSharp;
- contrato del runtime externo, Runtime ABI 1.0/symbol set 2 y verificación de `.abi`/`.lib`;
- fachada NativeSharp de filesystem, procesos, reloj y archivo mapeado Win32 de sólo lectura;
- adaptadores separados para `lld-link` y NSLink, quoting de Windows y response files;
- opción pública read-only `nsc --verify-pe <file.exe>` sobre el verificador existente;
- CTest acumulativo, prueba negativa de versión y evidencia estructurada del Gate SH03.

### No incluido

- portar todavía lexer, parser, semántica, MIR o lowering del compilador;
- declarar las 219 funciones en NativeSharp: el inventario completo está congelado, mientras la
  reimplementación sólo expone las 42 ya requeridas y ampliará contrato y prueba por necesidad;
- reescribir NSLink o NSAsm: una versión NativeSharp deberá vivir en repositorios nuevos;
- hacer al runtime interno al ejecutable: su independencia es precisamente el contrato certificado;
- soporte de LLVM 22, 24 u otra major; se rechaza con `NSH0301`.

## 3. Diseño

### Arquitectura

`nsc0.exe` se inspecciona como PE para obtener sus imports de `LLVM-C.dll`. El contrato conserva ese
conjunto y los hashes de las fuentes ABI NativeSharp. `EmitMinimalObject`, compilado por `nsc0`,
carga LLVM-C, construye el IR de `mainCRTStartup`, llama `ExitProcess(42)`, verifica el módulo,
ejecuta la pipeline `verify`, crea/finaliza DIBuilder y emite COFF.

`LinkerAdapterSmoke` escribe una petición común en response file, pero selecciona una clase adapter
distinta por linker. Ambos consumen la import library de `KERNEL32.dll`, producen un PE con tabla de
imports, pasan `nsc --verify-pe` y terminan con 42. `CompilerServicesSmoke` prueba la fachada de
plataforma sin que una futura fase del frontend dependa de APIs concretas de la stdlib o Win32.

Flujo: inventario `nsc0` → contrato LLVM-C → emitter NativeSharp → COFF → adapter/respuesta →
`lld-link` y NSLink → verificador PE → ejecución 42. En paralelo: contrato runtime → `.abi` y `.lib`
instalados → verificador `nsc`; y fachada Win32 → smoke de servicios.

### Contratos e invariantes

- LLVM debe ser 23.x; una major distinta produce `NSH0301` y salida 35 en la prueba negativa;
- todos los handles LLVM son opacos (`void*`) y sus layouts nunca se reproducen;
- `extern C` fija `win64-c`; los valores enum coinciden con headers LLVM 23;
- los 42 símbolos del binding son subconjunto exacto de los 219 imports auditados;
- cambiar enums o firmas cambia su SHA-256 canónico UTF-8/LF y hace fallar el contrato;
- cada handle LLVM creado se libera en el camino normal y en cada salida de error;
- el runtime externo satisface Runtime ABI 1.0, symbol set 2 y trece símbolos bootstrap;
- el puntero de `CompilerMappedFile.Data()` es prestado y muere antes de desmontar la vista;
- el response file cita todas las rutas y soporta espacios, barras finales y comillas;
- ambos linkers reciben el mismo objeto, import library, entry point, subsystem y machine;
- éxito observable: linker 0, verificador PE 0, ejecutable 42.

### Decisiones y alternativas

| Decisión | Alternativas consideradas | Razón |
|---|---|---|
| API C directa de LLVM | wrapper C++ existente | evita circularidad y deja ABI auditable |
| inventario completo + slice ejecutable | declarar 219 firmas sin consumidores | congela alcance real sin inventar binding no probado |
| hashes de fuentes ABI | duplicar firmas en JSON | una única definición compilable, verificada byte a byte |
| runtime externo versionado | incrustarlo o regenerarlo silenciosamente | separa compilador, runtime y compatibilidad ABI |
| fachada de plataforma pequeña | usar `File`/`Process` desde cada fase | permite cambiar implementación sin contaminar frontend |
| archivo mapeado Win32 real | leer todo a `byte[]` | cubre el patrón requerido por entradas grandes del compilador |
| adapters NativeSharp separados | condicionales CMake sin API | son código reutilizable por `nsc-next` y tienen misma prueba |
| response file común | concatenar todos los argumentos | evita límites de línea y fuerza quoting reproducible |
| exponer `--verify-pe` | helper de prueba nuevo | reutiliza el verificador de producción ya existente |

## 4. Inventario de archivos

| Archivo/familia | Acción | Responsabilidad | Razón |
|---|---|---|---|
| `lib/NativeSharp.LLVM.C/*` | crear | enums, firmas y política de versión LLVM | frontera directa y compilable |
| `lib/NativeSharp.Win32.Compiler/*` | crear | servicios de host, mapping y adapters | aislar plataforma/linkers |
| `selfhost/contracts/*` | crear | contratos LLVM-C/runtime y guía | congelar compatibilidad |
| `selfhost/bootstrap/verify_*_contract.py` | crear | validación cerrada y reportes | detectar drift antes de compilar frontend |
| `selfhost/tests/llvm/*` | crear | emitter, servicios y adapters | Gate SH03 ejecutable |
| `cmake/VerifyNativeSharpLlvmCBridge.cmake` | crear | orquestar el gate completo | misma prueba en ambas matrices |
| `cmake/VerifyNativeSharpBootstrapProfile.cmake` | modificar | incluir bindings y pruebas SH03 | impedir capacidades fuera del perfil |
| `src/Driver/Driver.cpp` | modificar | `--verify-pe` y help | verificador PE público reutilizable |
| `CMakeLists.txt` | modificar | registrar `nsc.selfhost.llvm-c-bridge` | baseline acumulativo |
| `tests/selfhost/test_bootstrap_infrastructure.py` | modificar | fijar contratos/hashes/cantidades | regresión rápida |
| READMEs y roadmap | modificar | navegación y límites | documentación operativa |
| este expediente y Gate SH03 | crear | diseño y evidencia | metodología por tanda |
| `MANIFEST_SHA256.txt` | modificar | integridad de fuentes cubiertas | cierre del commit |

## 5. Tipos y datos

| Tipo | Clase | Responsabilidad | Invariantes | Ownership/vida |
|---|---|---|---|---|
| enums `LLVM*` | enum ABI | niveles, reloc, model, file type, verifier | valores LLVM 23 exactos | valor |
| handles LLVM | `void*` opaco | contexto/módulo/builder/target/DI | layout desconocido | liberación explícita |
| `LLVMVersionPolicy` | class | aceptar sólo major 23 | diagnóstico `NSH0301` estable | sin estado |
| `CompilerMappedFile` | class | map read-only | vista válida sólo si `IsOpen()` | `raii`, posee 2 handles y vista |
| `CompilerClock` | class | reloj monotónico | elapsed nunca negativo | posee `Stopwatch` |
| `CompilerFileSystem` | class | IO/rutas mínimo | no expone Win32 | resultados/arrays según stdlib |
| `CompilerProcess` | class | proceso síncrono con log | conserva código hijo | sin estado |
| adapters de linker | class | respuesta y ejecución | éxito = código 0 | sin estado; archivos externos |
| contratos | JSON v1 | frontera congelada | schema/version/cantidades exactas | versionados |

## 6. Métodos y funciones

| Firma | Responsabilidad | Entradas y precondiciones | Salida y postcondiciones | Errores/diagnósticos | Efectos, ownership y concurrencia |
|---|---|---|---|---|---|
| `LLVMVersionPolicy.Read/IsSupported/Diagnostic` | consultar/aplicar versión | DLL cargable; punteros válidos | major 23 aceptada | `NSH0301` para incompatible | sólo lectura |
| `LLVM.LLVMContextCreate` … `LLVMDisposeDIBuilder` | slice LLVM-C | LLVM 23, handles correctos | módulo verificable/objeto COFF | mensajes LLVM se liberan | API C, single-thread |
| `CompilerFileSystem.*` | filesystem/rutas | rutas válidas | delega semántica stdlib | valores de fallo existentes | IO síncrono |
| `CompilerProcess.Run` | ejecutar herramienta | exe/línea/log | código exacto del hijo | -1 si no arranca | espera; stdout+stderr a archivo |
| `CompilerClock.ElapsedMilliseconds/Restart` | medir fases | instancia viva | tiempo monotónico | ninguno | estado local |
| `CompilerMappedFile` ctor/dtor | abrir/mapear/cerrar | archivo no vacío | vista read-only o cerrada | `IsOpen=false` | ownership RAII, single-thread |
| `CompilerCommandLine.Quote` | escape argv Windows | string válida | argumento entrecomillado reversible | ninguno | devuelve `owned string` |
| `WriteLinkResponse` | materializar petición | obj/lib/output | response file existente | false si IO falla | escribe archivo |
| `RunResponse` | lanzar linker | linker/rsp/log | código exacto | -1 si proceso falla | proceso síncrono |
| `LldLinkAdapter.Link` | adapter lld | seis rutas | PE o código de fallo | log persistente | escribe rsp/log/output |
| `NsLinkAdapter.Link` | adapter NSLink | seis rutas | PE o código de fallo | log persistente | escribe rsp/log/output |
| `verify_llvm_c_contract.main` | inventario/hashes | nsc/readobj/contrato | reporte JSON | `NSH0302` | proceso read-only + reporte |
| `verify_runtime_bootstrap_contract.main` | runtime externo | prefijo/nsc/contrato | reporte JSON | `NSH0303` | verifica `.abi` y `.lib` |
| `nsc --verify-pe` | verificar ejecutable | ruta PE | machine/secciones/imports | `PE verification failed` | read-only |

## 7. Superficie observable

- CTest nuevo: `nsc.selfhost.llvm-c-bridge`;
- CLI nueva: `nsc --verify-pe <file.exe>`;
- diagnósticos de gate: `NSH0301`, `NSH0302`, `NSH0303`;
- contratos JSON v1 LLVM-C y runtime-bootstrap;
- reportes JSON v1 LLVM-C, runtime y SH03;
- proyectos públicos `NativeSharp.LLVM.C.nsproj` y `NativeSharp.Win32.Compiler.nsproj`;
- no cambia el Runtime ABI, el formato `.nsmeta`, el object ABI ni la semántica del lenguaje.

## 8. Implementación

Se extrajeron 219 imports de `LLVM-C.dll` desde el PE instalado y se cotejaron con LLVM 23.1.0 y
`LlvmCApi.h`. Primero se compiló un emitter mínimo que devolvía 42; el verificador PE reveló que un
PE sin imports no satisfacía el contrato de producción. El IR se corrigió para llamar
`ExitProcess(42)` y se generó una import library con `nslink --link --make-implib`.

Después se añadieron los servicios Win32 y sus pruebas. Dos errores de lenguaje se resolvieron sin
ensanchar el perfil: imports `extern C` deben ser públicos y un campo `owned` se destruye
automáticamente. Finalmente se implementaron adapters NativeSharp con response files, se expuso el
verificador PE existente y se consolidó todo en un CTest. La ejecución específica final pasó con
ambos linkers y con rutas que contienen espacios.

## 9. Pruebas

| Prueba | Tipo | Propósito | Resultado | Evidencia |
|---|---|---|---|---|
| inventario PE de `nsc0` | contrato | congelar imports LLVM-C | PASS, 219 | reporte LLVM-C |
| hashes/fuentes binding | regresión | impedir drift de firma/enum | PASS, 42 | unit + reporte LLVM-C |
| versión incompatible | negativa | estabilizar política | PASS, `NSH0301`, exit 35 | CTest SH03 |
| emitter LLVM-C | positiva | emitir COFF sin frontend C++ | PASS, exit 42 | reporte SH03 |
| adapters con espacios/rsp | diferencial | enlazar misma entrada | PASS × 2 | reporte SH03 |
| ejecución PE | diferencial | observar semántica | PASS, 42 × 2 | CTest SH03 |
| `--verify-pe` | regresión | usar verificador existente | PASS, import count 1 × 2 | CTest SH03 |
| runtime `.abi`/`.lib` | contrato | certificar runtime externo | PASS | reporte runtime |
| servicios de compilador | positiva | filesystem/proceso/map/reloj | PASS, exit 42 | CTest SH03 |
| Bootstrap Profile ampliado | regresión | revisar nuevas fuentes | PASS | CTest profile |
| Python selfhost | unit | schemas, hashes, cantidades | PASS, 24/24 | unittest |

## 10. Comparación diferencial

Los dos linkers reciben bytes COFF idénticos y una import library idéntica. Los PE no se comparan
byte a byte: `lld-link` conserva timestamp y el layout puede diferir del layout determinista de
NSLink. La equivalencia exigida es machine AMD64, PE32+, entry point válido, una importación
`KERNEL32.dll!ExitProcess`, aprobación del mismo verificador y retorno 42.

El emitter NativeSharp no se compara todavía contra el lowering completo de `nsc0`: SH03 prueba la
frontera, no equivalencia del compilador. Ese diferencial comienza al portar MIR/codegen.

## 11. Baseline de cierre

El baseline completo se ejecutó después del último cambio funcional con la configuración canónica:

```text
python -B nscbuilder.py --baseline --config selfhost/bootstrap/bootstrap.config.example.json --run-id tanda-04-sh03-final-20260919 --jobs 16
```

Resultado global: `PASSED`, cero fallos, 463.737 segundos.

| Matriz/prueba | Resultado |
|---|---:|
| CTest con `lld-link` | 440/440 PASS |
| CTest con NSLink | 440/440 PASS |
| fixtures diferenciales | 296/296 PASS (148 por linker) |
| triage diferencial | 36/36 PASS (18 por linker) |
| gates self-hosting acumulativos | 10/10 PASS (5 por linker) |
| Python selfhost | 24/24 PASS |
| Gate SH03 | PASS en ambas matrices; retorno 42 con ambos linkers |

| Artefacto | SHA-256 |
|---|---|
| `report.md` | `287ec95047d6f46c68d815cc1316ef4b4953db231b7a5c3fef48e505bfba21ec` |
| `report.json` | `e7750d15a1463d5cc91c8556dfabd823fe9dab81d22827110f79c04e8755247b` |
| digest de fuentes | `781ad89506fd6c20f738cd871fd6f5641f2076f56235e3d1c63f4274873a8f8a` |
| `nsc0` construido/instalado | `d2eeb7abcadb802a1bf2802a0fef148d62f666bfa6e53ee70ec02b5c288f6561` |
| Gate G00 | `d3aeefe55bfc192b7efbcdc732e62fc83f10e3082a918f3beae968fc93230612` |
| reporte SH03 | `25ccf3ad75e909defb71e285cf5d4076c717d944cb9e0b0c37dbe8c3a7dbeedc` |
| reporte contrato LLVM-C | `016c961f366b08e8689e9c4ec3c13b3a365fd882bde962df6a76b976509a18cc` |
| reporte contrato runtime | `490a1b867e19f6cf4ba2343f3d0921d1b3c35f43c15ed5b975d7380d729ebe4a` |

Evidencia normativa: `selfhost/evidence/runs/tanda-04-sh03-final-20260919/` y
`selfhost/evidence/gates/SH03/`. No hubo una corrida final fallida que conservar; las correcciones
descritas en la sección 8 ocurrieron durante la implementación, antes de este baseline de cierre.

## 12. Riesgos, limitaciones y deuda

- sólo x86_64 Windows está certificado; los cinco imports AArch64 de `nsc0` están inventariados pero
  no ejercitados por este gate;
- el binding de 42 símbolos es suficiente para el objeto mínimo, no para el lowering completo;
- `CompilerMappedFile` es read-only y no representa archivos vacíos como una vista abierta;
- los adapters son síncronos y todavía no tienen cancelación/timeout, coherente con SH01;
- LLVM minor/patch son informativos: compatibilidad se decide por major 23;
- el contrato usa hashes de fuentes para exigir revisión explícita; cualquier edición legítima
  debe actualizar contrato y prueba en el mismo commit.

## 13. Contenido del commit

### Archivos incluidos

Los archivos incluidos son exactamente los enumerados en la sección 4 y los artefactos de cierre
que se añadirán tras el baseline. El staging excluye cambios locales anteriores del usuario.

### Higiene

- [x] No contiene `.obj`, `.exe`, `.dll`, `.lib`, PDB, temporales ni cachés no normativos.
- [x] No contiene cambios locales ajenos a la tanda.
- [x] El manifiesto de fuentes está actualizado cuando corresponde.
- [x] La documentación describe las firmas y archivos realmente incluidos.
- [x] El baseline se ejecutó después del último cambio funcional.

### Commit de cierre

```text
selfhost(tanda-04): freeze LLVM and platform boundaries
```

## 14. Criterio de cierre

- [x] `NS2-SH-030` a `NS2-SH-044` implementados.
- [x] Pruebas específicas verdes con ambos linkers y retorno 42.
- [x] Baseline completo verde con ambos linkers.
- [x] Gate SH03 `LANDED`, enlazado a SH02 y sin problemas.
- [x] Expediente, manifiesto, evidencia y commit coinciden.
