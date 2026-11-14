# Tanda 9: validación agrupada 2026-11-14

Compilador `A:/nsc-devel/build-tanda09/bin/nsc.exe`
(`3.0.0+build.20261114T160753Z.d3d9c0dc5fdc10ba`), Clang 23.1.0 en `C:/clang-llvm`,
`NSC_LINKER=nslink.exe`, runtime `build-tanda09/generated/NativeSharp.Runtime.Win32.lib`,
TEMP en `A:/temp`. Una sola corrida del conjunto de
[TANDA-09-VALIDACION.md](TANDA-09-VALIDACION.md) más los controles nuevos de esta tanda
(`build-claude-t08/tools/validacion-t09.cmd`, registros `build-claude-t08/logs/vt09-*.log`).

| Validador | Resultado | Evidencia |
|---|---|---|
| `pruebas-tanda-09-llamadas/validar.py` (8 C/NS) | pasa | `A:/temp/nsc-t09-llamadas-eh_064lw` |
| `pruebas-tanda-09-agregados/validar.py` (8 C/NS) | pasa | `A:/temp/nsc-t09-agregados-tqn_84ac` |
| `pruebas-tanda-09-vectorcall/validar.py` (8 C/NS) | pasa | `A:/temp/nsc-t09-vectorcall-3s9bko1x` |
| `pruebas-tanda-09-llamadas/validar-hir-mir.py` | pasa (estática y DLL) | registro `vt09-03.log` |
| `pruebas-tanda-09-llamadas/validar-llvm.py` | pasa (estática y DLL) | registro `vt09-04.log` |
| `pruebas-tanda-09-sysv/validar.py` | **falla en el control negativo** | `A:/temp/nsc-t09-sysv-prueba-4ioyds3h` |
| `pruebas-tanda-09-global-const/validar.py` | pasa | registro `vt09-06.log` |
| `pruebas-tanda-09-simbolo-ausente/validar.py` | pasa | registro `vt09-07.log` |
| `pruebas-tanda-09-biblioteca-ausente/validar.py` | pasa | `A:/temp/nsc-t09-lib-ausente-9kb4qrm9` |
| `pruebas-tanda-09-precompilado/validar.py` (16 consumidores) | pasa | `A:/temp/nsc-t09-precompilado-sy6qhvma` |
| `pruebas-tanda-09-cache/validar.py` | pasa | `A:/temp/nsc-t09-cache-bxulbf6j` |
| `pruebas-tanda-09-cache-biblioteca/validar.py` | pasa | `A:/temp/nsc-t09-cache-biblioteca-oh_fizh1` |
| `pruebas-tanda-09-convenciones/validar-matriz.py` | pasa | `A:/temp/nsc-t09-convenciones-73ucdhru` |
| `pruebas-tanda-09-preserve-none-c-plan/validar.py` | pasa | `A:/temp/nsc-t09-preserve-none-c-plan-4i6558gy` |
| CTest project-loader, item331, item332, item333, item344, item345 | pasan | `vt09-ctest-*.log` |
| CTest clang-abi-probe, regcall-record-plan, 3 variadic-pointer | pasan | `vt09-ctest-*.log` |
| `nsc_cpp_c_identity_tests` | pasa | `vt09-identity.log` |

## Falla del control negativo SysV

Las ocho comparaciones C/NS y la plantilla precompilada de `pruebas-tanda-09-sysv`
pasan. Falla solamente su control negativo: espera que NSC rechace
`__attribute__((sysv_abi)) T09Unsupported sysv_record(float, T09Other, float, ...)`
con `sysv_abi currently supports only selected`, y NSC ahora la acepta.

HECHO COMPROBADO: el IR de NSC (`no-admitido.log`) emite
`call x86_64_sysvcc void (ptr, float, i64, i32, float, ...) @sysv_record(ptr noalias
sret([20 x i8]) align 4 ...)`, es decir, retorno indirecto del record de 20 bytes y
`T09Other` partido en `i64` e `i32`, que es la clasificación SysV del record de 12 bytes.

INFERENCIA: la aceptación proviene de un avance anterior de T09-003 ya registrado en el
tracking (retorno indirecto de record C en llamadas variádicas SysV con agregado fijo
de entrada, plan Clang validado). Los cambios de esta sesión no tocan la ruta SysV.
NO VERIFICADO: ejecución del caso contra una implementación C; el fixture no la
proporciona.

Clasificación: prueba obsoleta, no fallo funcional. La expectativa negativa pertenece a
una prueba existente; las instrucciones vigentes impiden modificarla sin autorización.
Queda **BLOQUEADO** hasta que el usuario autorice sustituir la firma negativa por otra
aún no admitida, o convertir el caso en comparación positiva contra Clang.

## Estado

23 de 24 validadores pasan. El cierre formal de la tanda queda bloqueado por el control
negativo anterior. No se ejecutaron los gates completos de la tanda 8 ni `stage48_2`;
la suite de sesión Clang sigue sin su fixture en esta máquina, como en la validación
anterior.

## Resolución y segunda corrida (2026-11-14)

El usuario indicó convertir el caso SysV en comparación positiva contra Clang y mantener
un control negativo separado. Detalle en
[registro-commits/t09-sysv-record-variadico-positivo.md](registro-commits/t09-sysv-record-variadico-positivo.md).
Segunda corrida del mismo conjunto con el mismo compilador: **24 de 24 validadores
pasan**.

| Validador | Evidencia |
|---|---|
| llamadas | `A:/temp/nsc-t09-llamadas-b_hbwei9` |
| agregados | `A:/temp/nsc-t09-agregados-0b3683f1` |
| vectorcall | `A:/temp/nsc-t09-vectorcall-q70iyk2l` |
| sysv (con el record variádico y los dos rechazos) | `A:/temp/nsc-t09-sysv-prueba-e87jwwi3` |
| precompilado | `A:/temp/nsc-t09-precompilado-jeu_8s64` |
| cache | `A:/temp/nsc-t09-cache-5qbg0l3w` |
| cache-biblioteca | `A:/temp/nsc-t09-cache-biblioteca-0lwzkafc` |
| matriz de convenciones | `A:/temp/nsc-t09-convenciones-ufcy9845` |
| preserve_none C por plan | `A:/temp/nsc-t09-preserve-none-c-plan-8ykp3sfs` |
| biblioteca ausente | `A:/temp/nsc-t09-lib-ausente-k6fykyeg` |

HIR/MIR, LLVM, global const, símbolo ausente, los nueve CTest y la identidad C pasan.
Están registrados en `build-claude-t08/logs/vt09-*.log`.

La tanda 9 **sigue abierta**: T09-003 conserva capacidades pendientes sin evidencia.
Son las firmas fuera de los planes validados, los records C por valor con
`preserve_none` y las convenciones `swiftcall`, `swiftasynccall` e `intel_ocl_bicc`.
