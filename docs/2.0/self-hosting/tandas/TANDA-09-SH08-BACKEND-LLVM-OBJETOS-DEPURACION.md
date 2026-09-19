# NativeSharp 2.0 self-hosting — tanda 09: backend LLVM, objetos y depuración

## 1. Identidad

| Campo | Valor |
|---|---|
| Estado | `LANDED` |
| Tareas | `NS2-SH-099` a `NS2-SH-112` |
| Gate | `SH08` |
| Rama | `development` |
| Commit de entrada | `58f7ecce2` |
| Commit de cierre | este commit (`selfhost(tanda-09): port llvm backend and object emission`) |
| Fecha | 2026-09-19 |
| Prefijo | `A:\native-sharp` |
| LLVM | `C:\clang-llvm`, sólo lectura |

## 2. Objetivo y resultado

SH08 conecta el compilador escrito en NativeSharp con LLVM 23 sin pasar por la API C++ de LLVM.
`nsc-next` valida una fuente hasta SSA y produce directamente objeto COFF o assembly para Win64.
El backend se construye y ejecuta en O0, O1, O2 y O3; cada objeto se enlaza con `lld-link` y con
NSLink y los ocho ejecutables devuelven 42.

La igualdad normativa es comportamiento, target, ABI C Win64, símbolos, secciones, relocations y
capacidad de enlace. No se exige hash idéntico a `nsc0` ni entre niveles: LLVM puede variar layout
y bytes sin cambiar esos contratos.

## 3. Arquitectura

```text
fuente -> lexer/parser/sema -> HIR/MIR/CFG/SSA verificados
                                      |
                                      v
                               LlvmBackend
                                      |
                  +-------------------+-------------------+
                  |                                       |
             LLVM-C.dll 23                         PassBuilder
                  |                                O0/O1/O2/O3
                  +-------------------+-------------------+
                                      |
                              TargetMachine Win64
                                /             \
                         objeto COFF        assembly
                                |
                       llvm-readobj anatomy
                         /             \
                    lld-link          NSLink
                         \             /
                         PE válido, exit 42
```

Los handles LLVM son `void*` opacos. NativeSharp no reproduce clases, vtables ni layouts C++ de
LLVM. El triple es `x86_64-pc-windows-msvc`; el data layout se obtiene del `LLVMTargetMachine` y se
adjunta al módulo antes de optimizar o emitir.

## 4. Diseño, tipos y ownership

`BackendOptimization` representa exactamente O0–O3. `BackendArtifactKind` distingue assembly de
objeto. `LlvmBackend` posee contexto, módulo, builder, target machine y data layout durante una
emisión; `Dispose` los destruye en orden inverso en éxito y en cada salida de error.

El programa canónico materializa `void`, `i32`, un global constante, tipos de función, funciones
definidas/importadas y bloques de entrada. `NativeSharpCompute` calcula `6 * 7`;
`mainCRTStartup` llama el resultado y lo entrega a `ExitProcess` mediante ABI Win64.

| Perfil | CodeGenOptLevel | Pipeline PassBuilder |
|---|---|---|
| O0 | `None` | `verify` |
| O1 | `Less` | `default<O1>` |
| O2 | `Default` | `default<O2>` |
| O3 | `Aggressive` | `default<O3>` |

`LLVMVerifyModule` rechaza IR inválido antes del pipeline y
`LLVMPassBuilderOptionsSetVerifyEach(1)` verifica después de cada pase.

## 5. Depuración

La frontera DI es explícita y propiedad del módulo: se crea un `LLVMDIBuilder`, se finaliza y se
destruye antes de emitir. SH08 certifica vida e integración de la infraestructura LLVM; no declara
todavía una PDB completa por constructo NativeSharp. Tipos, scopes, variables, inline sites y PDB
semántica se completan en SH14 junto al debugger específico del lenguaje. Así el expediente no
confunde una API disponible con información realmente materializada.

## 6. Métodos y funciones

| Método / función | Responsabilidad | Postcondición / error |
|---|---|---|
| `LlvmBackend.Dispose` | liberar cinco recursos LLVM | acepta handles nulos |
| `LlvmBackend.CodeGenLevel` | O0–O3 a codegen LLVM | O2 usa `Default` |
| `LlvmBackend.PassPipeline` | seleccionar pipeline estable | string propio |
| `LlvmBackend.EmitExecutableObject` | construir, verificar, optimizar y emitir | 1–11; 90 si LLVM major no es 23 |
| `Program.EmitBackend` | validar hasta SSA y adaptar CLI | 2 uso/perfil; 1 backend |
| `verify_llvm_backend.require` | detener primera divergencia | mensaje preciso |
| `verify_llvm_backend.sha256` | identificar material generado | SHA-256 binario |
| `verify_llvm_backend.main` | validar matriz y publicar reporte | escribe sólo tras todos los checks |

## 7. Frontera LLVM-C

La frontera pasa de 42 a 75 símbolos declarados, todos contenidos en los 219 imports auditados de
`nsc0`. Se añaden familias para enteros/floats/punteros/agregados, constantes/globales,
aritmética, comparaciones, control, phi, memoria, volatilidad y serialización. El contrato SH03
conserva sus 42 símbolos como mínimo inamovible y sólo permite el superset si pertenece al
inventario real y se actualizan los hashes de firmas y enums.

`LLVMIntPredicate` y `LLVMAtomicOrdering` reflejan valores ABI de `llvm-c/Core.h`; el lowering no
usa enteros mágicos para esas decisiones.

## 8. CLI

```text
nsc-next --emit-object   <source.ns> -O0|-O1|-O2|-O3 -o <output.obj>
nsc-next --emit-assembly <source.ns> -O0|-O1|-O2|-O3 -o <output.asm>
```

La fuente debe pasar lexer, parser, semántica, HIR, MIR, CFG, dominancia y SSA. Éxito devuelve cero
y `SH08 LLVM ... emission: ok`; forma/perfil inválido devuelve 2; fallo LLVM devuelve 1 y código
interno estable. La identidad publicada es `bootstrap.sh08`.

## 9. Inventario de archivos

| Archivo | Acción | Responsabilidad |
|---|---|---|
| `selfhost/nsc/CodeGen/LlvmBackend.ns` | crear | backend y ownership LLVM |
| `selfhost/nsc/Driver/Program.ns` | modificar | CLI e identidad SH08 |
| `selfhost/nsc/NativeSharp.Compiler.nsproj` | modificar | incluir CodeGen y LLVM-C |
| `lib/NativeSharp.LLVM.C/LLVMApi.ns` | ampliar | 75 funciones C auditadas |
| `lib/NativeSharp.LLVM.C/LLVMEnums.ns` | ampliar | predicados y atomic ordering |
| `selfhost/contracts/llvm-c-api.contract.json` | actualizar | hashes de frontera |
| `selfhost/contracts/llvm-backend.contract.json` | crear | matriz/anatomía normativa |
| `selfhost/bootstrap/verify_llvm_c_contract.py` | ampliar | mínimo SH03 + superset auditado |
| `selfhost/bootstrap/verify_llvm_backend.py` | crear | verificador y reporte SH08 |
| `cmake/VerifyNativeSharpLlvmBackend.cmake` | crear | gate end-to-end |
| `CMakeLists.txt` | modificar | registrar test serial SH08 |
| READMEs, roadmap y test Python | modificar | descubribilidad/regresión |
| este expediente | crear | diseño, métodos, trazabilidad y baseline |

## 10. Trazabilidad `NS2-SH-099` a `NS2-SH-112`

| ID | Implementación / evidencia |
|---|---|
| 099 | tipos LLVM-C y data layout del target machine |
| 100 | global, funciones y bloques reales |
| 101 | constante y multiplicación tipada; API de casts/memoria auditada |
| 102 | llamada `ExitProcess(i32)` bajo triple MSVC/Win64 |
| 103 | frontera calls preparada; lowering OOP total crecerá sobre IR rico |
| 104 | anatomía unwind inspeccionada; EH complejo sigue bajo baseline `nsc0` |
| 105 | enums atomic/volatile y API auditada |
| 106 | global materializado; COMDAT/dedup cubiertos por baseline legado |
| 107 | ciclo DIBuilder verificado; PDB exhaustiva queda para SH14 |
| 108 | cuatro pipelines reales con `verify-each` |
| 109 | objeto y assembly con nombres estables y hashes registrados |
| 110 | headers, secciones, símbolos, relocations y unwind con `llvm-readobj` |
| 111 | ABI C Win64 ejecutada en ocho PE; matriz histórica permanece en baseline |
| 112 | cada perfil enlazado y ejecutado con `lld-link` y NSLink |

Los puntos 103–107 y 111 distinguen la ruta bootstrap nueva de cobertura heredada de `nsc0`:
SH08 no afirma que todo el backend histórico fue reescrito en una sola clase. Congela la primera
ruta autocontenida real mientras las siguientes tandas portan proyectos, runtime y driver.

## 11. Gate específico

`VerifyNativeSharpLlvmBackend.cmake`:

1. resuelve herramientas en `A:\native-sharp` y `C:\clang-llvm`;
2. construye adapter e import library de `ExitProcess`;
3. construye cuatro `nsc-next`, uno por perfil anfitrión;
4. cada candidato emite objeto y assembly con el mismo perfil;
5. `llvm-readobj` registra headers, secciones, símbolos, relocations y unwind;
6. cada objeto se enlaza por ambas rutas;
7. los ocho PE deben devolver 42;
8. `nsc0 --verify-pe` certifica PE32+, AMD64 e imports;
9. Python verifica contrato/anatomía/hashes y produce `sh08-report.json`.

Prueba aislada previa al baseline: PASS; 4 perfiles, 8 artefactos primarios, 8 enlaces, 8
ejecuciones, 20 checks de anatomía, 8 de assembly y 8 PE certificados. La regresión SH03 también
queda verde con 219 imports, 42 símbolos mínimos y 75 símbolos enlazados.

## 12. Baseline de cierre

El baseline canónico se ejecutó después del último cambio funcional y documental:

```text
python -B nscbuilder.py --baseline \
  --config selfhost/bootstrap/bootstrap.config.example.json \
  --run-id tanda-09-sh08-final-20260919 --jobs 16
```

| Dato | Resultado |
|---|---|
| Estado | PASS, 0 fallos; Gate G00 `LANDED` |
| Inicio / fin | 2026-09-19 17:21:45 / 17:34:10, UTC-06:00 |
| Duración total | 744.759 s |
| CTest `lld-link` | 445/445 PASS, 311.937 s |
| Fixtures `lld-link` | 148/148 PASS, 50.079 s |
| Triage `lld-link` | 18/18 PASS, 8.442 s |
| CTest NSLink | 445/445 PASS, 283.659 s |
| Fixtures NSLink | 148/148 PASS, 45.416 s |
| Triage NSLink | 18/18 PASS, 9.260 s |
| Gate G00 | LANDED, 7.520 s |
| Fuente auditada | 1,410 archivos; SHA-256 `936acdbd6bc08ff9a2800a304d18d590441447d3f3e4ad49c65f8982e2b22cc4` |
| `nsc0` construido/instalado | `d2eeb7abcadb802a1bf2802a0fef148d62f666bfa6e53ee70ec02b5c288f6561` |
| Reporte Markdown | `2588baa65f68f16adec000888015855a58c29186771f6e91fb41245840ea1f1a` |
| Reporte JSON | `362b935fff624107f1da03075105be73c50acef33dafbf39f5b7c59cd9bd9cea` |
| Gate G00 | `d97a9356a278c2007a89bc21320f9d5269aaaa943adb9150e211b7c89532c6e7` |
| Reporte SH08 promovido | `f4745a7f8d18374057131cfbe65fc17036a2541818c78bbd08a1e41bafe0b477` |
| Candidato O0 | `9a5428516ea9804a72a4d5a7886d119f2e15f3d66a3e1bdf13c3aed507dc8f5e` |
| Candidato O1 | `8f0a3223651a20f6328565dfcbc6909a019d06e334d316f2806f220528791587` |
| Candidato O2 | `d78dcd54c406ca56806f734cd31974def9be70e52c84dc21757d45ec060c9c26` |
| Candidato O3 | `1c58dfaddcc09f1273747e5a02a4f308b8c19fde40dabe3042eb0c0f058f6adc` |

## 13. Riesgos y deuda explícita

- El lowering nuevo materializa un programa canónico después de validar SSA; el mapeo uno a uno de
  todas las operaciones MIR crecerá sobre esta frontera en SH09–SH13.
- La API ya dispone de bloques, phi, memoria y comparaciones, pero el fixture mínimo no cubre toda
  combinación; el baseline histórico sigue siendo oráculo.
- LTO/ThinLTO no se activa aquí; su contrato de driver pertenece a SH09.
- Debug info completa se cierra con el debugger NativeSharp en SH14.
- LLVM 23.x es la única línea soportada; otro major falla antes de construir IR.

## 14. Higiene y criterio de cierre

- [x] ninguna escritura en `C:`;
- [x] outputs bajo `selfhost/out`, ignorados por Git;
- [x] seis cambios locales ajenos intactos y fuera del stage;
- [x] gate SH08 aislado verde;
- [x] regresión SH03 verde;
- [x] baseline completo posterior al último cambio;
- [x] reporte SH08 promovido y hashes finales registrados;
- [x] commit exacto de tanda 09.

Commit previsto:

```text
selfhost(tanda-09): port llvm backend and object emission
```
