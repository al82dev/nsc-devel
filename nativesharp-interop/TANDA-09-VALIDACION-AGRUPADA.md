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

## Regresión completa tras los commits 4555d08fb, bd3da69bc y 5931cb476 (2026-11-14)

Compilador `A:/nsc-devel/build-tanda09/bin/nsc.exe` (build hash `a07da8268ce1d931`, HEAD
`5931cb476`), Clang 23.1.0, `NSC_LINKER=nslink.exe`, runtime
`build-tanda09/generated/NativeSharp.Runtime.Win32.lib`. Herramienta
`build-claude-t08/tools/regresion-t09.py "pruebas-tanda-09-*"` con cinco procesos en
paralelo, agrupados por directorio (`lote-t09-final.cmd`). Registros por guion en
`build-claude-t08/logs/regresion-t09-final/` y resumen en
`build-claude-t08/logs/regresion-t09-final.log`.

Resultado: **1090 guiones `validar*.py`; 1084 pasan, 4 fallan y 2 se omiten.**

| Guion | Resultado | Clasificación |
|---|---|---|
| `pruebas-tanda-09-cpp-variadica-preserve-none/validar.py` | falla | Falla el control de Clang 23.1.0, no NSC. Ya documentado en el fixture: Clang no produce una llamada variádica `preserve_none` utilizable. Se mantiene fuera de alcance. |
| `pruebas-tanda-09-cpp-variadica-preserve-none/validar-precompilado.py` | falla | La misma causa. |
| `pruebas-tanda-09-cpp-tamano-objeto/validar-indice-expresion.py` | falla en la corrida paralela; pasa al repetirse | Intermitente; ver abajo. |
| `pruebas-tanda-09-cpp-tamano-objeto/validar-puntero-desplazado.py` | falla en la corrida paralela; pasa al repetirse | Intermitente; ver abajo. |
| `…-cpp-variadica-sysv-record/validar-rechazos-precompilado.py` y `…-cpp-variadica-sysv-record-indirecta/validar-rechazos-precompilado.py` | omitidos | Exigen `--caso` y `--mutador`. Los invocan sus guiones padre, que pasan. |

Los 14 guiones precompilados `sysv-indirecta-*` y los controles negativos convertidos
en esta sesión pasan todos. Eran regcall, regcall-dos-records, sysv-indirecta,
el transporte variádico y preserve-none-record.

### Intermitencia en `cpp-tamano-objeto`

HECHO COMPROBADO: en la corrida paralela, ambos guiones fallaron en la variante DLL O1.

- `validar-indice-expresion` falló en `O1-variable-anidado-size1`, con el control Clang
  en 0 y NativeSharp en 4294967295. Evidencia: `A:/temp/nsc-t09-object-size-47ljl4yd`.
- `validar-puntero-desplazado` falló en `dll-O1-indice`. Evidencia:
  `A:/temp/nsc-t09-pointer-offset-nwnas15p`.

Repetidos con el mismo compilador y un solo proceso, ambos pasan
(`lote-tamano.cmd`; `logs/regresion-t09-tamano.log`: 40 s y 110 s). En la regresión
completa anterior también pasaban.

Tras el fallo se volvieron a ejecutar 15 veces los dos ejecutables que dejó la
evidencia, `variable.exe` (Clang) y `variable-ns.exe` (NativeSharp). Ambos terminan
siempre en -1 (`tools/varios2.cmd`), así que coinciden entre sí. El valor distinto lo
dio el control de Clang en la corrida paralela, no el ejecutable de NSC.

INFERENCIA: es una interferencia del entorno bajo carga. La más probable es que se
ejecutara un `variable.exe` anterior, cuando el enlace del control se cruzó con la
variante previa.

NO VERIFICADO: la causa exacta. No se modificó ningún fixture por esto.

### Estado

La tanda 9 **sigue abierta**. T09-003 conserva capacidades sin evidencia:

- enums C por valor con regcall, sysv_abi, preserve_* y las convenciones por plan;
- uniones C por valor en punteros sysv_abi;
- réplica precompilada de campos `volatile`, `const` y `_Alignas` en records de alias.

## Regresión completa con enums C por valor (2026-11-14)

Compilador con los cambios de
[registro-commits/t09-enum-c-convenciones.md](registro-commits/t09-enum-c-convenciones.md),
sobre `825386d84`. Misma herramienta y entorno que la corrida anterior
(`lote-t09-enum.cmd`). Registros en `build-claude-t08/logs/regresion-t09-enum/` y
resumen en `logs/regresion-t09-enum.log`.

Resultado: **1091 guiones; 1085 pasan, 4 fallan y 2 se omiten.** Hay un guion más que en
la corrida anterior: el fixture nuevo `pruebas-tanda-09-c-enum-convenciones/validar.py`,
que pasa.

- Fallan los dos guiones de Clang `cpp-variadica-preserve-none`, por la misma causa
  documentada.
- Fallan otra vez dos guiones de `cpp-tamano-objeto` solo en la corrida paralela, ahora
  `validar-indice-expresion` y `validar-indice-llamada`. Es el mismo patrón en DLL: el
  control de Clang da 0 y NativeSharp 4294967295 (`O1-variable-suma-size1` y
  `O3-variable-anidado-size1`). Repetidos con un solo proceso, ambos pasan
  (`logs/regresion-t09-enum-reintento/resumen-reintento.txt`: 39 s y 31 s).
  Estos guiones son C++ `cdecl` y no tocan las rutas cambiadas. Se mantiene la
  clasificación de intermitencia del entorno, con la causa NO VERIFICADA.
- Los dos guiones convertidos pasan: `pruebas-tanda-09-regcall/validar-negativo.py` y
  `pruebas-tanda-09-regcall-dos-records/validar.py`, con y sin precompilado.
- Pasan también:
  - la matriz de firmas, 252/252 (`A:/temp/nsc-t09-matriz-firmas-2opw4bzl`);
  - la matriz de convenciones;
  - las convenciones por plan;
  - `preserve-none-c-plan`;
  - los fixtures de enums SysV con records, C y C++ variádicos.

`lote-ct2.cmd` con el mismo compilador da estos resultados:

- Pasan los nueve CTest: `clang-abi-probe`, `regcall-record-plan`, `variadic-pointer`,
  `project-loader` y los `item331` a `item345`.
- Pasa `nsc_cpp_c_identity_tests`.
- `ninja -k 0` solo muestra los errores de enlace previos de los ejecutables de prueba:
  `PrecompiledSemanticSnapshotCodec::decode`, `cppCInlineLinkSymbol` y
  `projectCppCxxEnumConstant`.

La tanda 9 **sigue abierta**. T09-003 conserva estas capacidades sin evidencia:

- uniones C por valor en punteros `sysv_abi`;
- réplica precompilada de campos `volatile`, `const` y `_Alignas`;
- enums C++ por valor con convenciones no `cdecl`.

## Regresión completa con uniones C por valor (2026-11-14)

El compilador incluye los cambios de
[registro-commits/t09-union-c-convenciones.md](registro-commits/t09-union-c-convenciones.md)
sobre `fe8b1accb` (`lote-t09-union.cmd`, registros en
`logs/regresion-t09-union/`). Resultado: **1092 guiones; 1087 pasan, 3 fallan y 2 se
omiten.**

- Pasan el fixture nuevo `pruebas-tanda-09-c-union-convenciones/validar.py` y el guion
  convertido `pruebas-tanda-09-sysv-indirecta/validar.py`.
- Fallan los dos de Clang `cpp-variadica-preserve-none`, por la causa ya documentada.
- Falla otra vez `cpp-tamano-objeto/validar-indice-expresion` en la corrida paralela, con el
  mismo patrón en DLL O2 `variable-anidado-dynamic1`: el control de Clang da 0 y
  NativeSharp 16. Se repitió con un solo proceso (`logs/regresion-t09-union-reintento/`).
  En esa repetición pasa (39 s).

Con el mismo compilador (`lote-ct2.cmd`, registros en `logs/`):

- `ninja -k 0` solo falla por los tres símbolos ya conocidos:
  `PrecompiledSemanticSnapshotCodec::decode`, `cppCInlineLinkSymbol` y
  `projectCppCxxEnumConstant`;
- pasan las 9 CTests (`clang-abi-probe`, `regcall-record-plan`, `variadic-pointer`,
  `project-loader`, `item331`, `item332`, `item333`, `item344`, `item345`);
- pasa `nsc_cpp_c_identity_tests`.

## Regresión completa con el replay de campos calificados (2026-11-14)

El compilador incluye los cambios de
[registro-commits/t09-replay-campos-calificados.md](registro-commits/t09-replay-campos-calificados.md)
sobre `9ccabd1bc` (`lote-t09-vol.cmd`, registros en `logs/regresion-t09-vol/`). Resultado:
**1093 guiones; 1087 pasan, 4 fallan y 2 se omiten.**

- Pasan el fixture nuevo `pruebas-tanda-09-precompilado-campos-calificados/validar.py` y los
  catorce `pruebas-tanda-09-sysv-indirecta-*/validar-precompilado.py` convertidos.
- Fallan los dos de Clang `cpp-variadica-preserve-none`, por la causa ya documentada.
- Fallan en la corrida paralela `cpp-tamano-objeto/validar-indice-expresion` y
  `validar-indice-llamada`. Repetidos con un solo proceso
  (`logs/regresion-t09-vol-reintento/`), pasan los dos. Es el patrón intermitente ya
  registrado; su causa sigue sin verificar.

Con el mismo compilador (`lote-ct2.cmd`, registros en `logs/`):

- `ninja -k 0` solo falla por los tres símbolos ya conocidos;
- pasan `nsc_cpp_c_identity_tests` y ocho de las nueve CTests;
- `clang-abi-probe` fallaba con el código 209: su caso `QualifiedField { const int x; }`
  exigía que un campo `const` no fuera reproducible, que es justo lo que cambia. Siguiendo la
  política acordada, ese caso pasa a positivo, se añaden `VolatileField` y
  `ConstVolatileArray` también positivos, y el rechazo se conserva con
  `UnalignedField { __unaligned int x; }`, un calificador que sigue fuera. Con esa
  actualización pasa (`logs/run-probe-test.log`).

La tanda 9 **sigue abierta**. T09-003 conserva estas capacidades sin evidencia:

- records C con records anidados en llamadas `sysv_abi` variádicas precompiladas;
- enums C++ por valor con convenciones no `cdecl`;
- la unión C solo `float` con `regcall`.

## Validación de records anidados en variádicas precompiladas (2026-11-14)

El compilador incluye los cambios de
[registro-commits/t09-variadica-records-anidados.md](registro-commits/t09-variadica-records-anidados.md)
sobre `8736cd9aa`. El cambio solo toca `planPrecompiledCppVariadicCall`, así que se repiten
los guiones que pasan por llamadas variádicas o plantillas precompiladas
(`lote-var-sub.cmd`):

- `pruebas-tanda-09-*variadic*` (`logs/regresion-t09-var-variadica/`): **191 guiones; 187
  pasan, 2 fallan y 2 se omiten.** Fallan solo los dos de Clang `cpp-variadica-preserve-none`,
  por la causa ya documentada. Pasa el fixture nuevo
  `pruebas-tanda-09-precompilado-variadica-anidado/validar.py`.
- `pruebas-tanda-09-precompilado*` (`logs/regresion-t09-var-precompilado/`): 3 de 3 pasan,
  entre ellos `pruebas-tanda-09-precompilado-campos-calificados/validar.py` con el record
  anidado `volatile` en la sección variádica.

Con el mismo compilador (`lote-ct2.cmd`): pasan las nueve CTests y
`nsc_cpp_c_identity_tests`; `ninja -k 0` solo falla por los tres símbolos ya conocidos.

La regresión completa se repite con el cambio siguiente de la tanda.

## Regresión completa con enums C++ por valor (2026-11-14)

El compilador incluye los cambios de
[registro-commits/t09-enum-cpp-convenciones.md](registro-commits/t09-enum-cpp-convenciones.md)
sobre `e1d59448e` (`lote-t09-ecpp.cmd`, registros en `logs/regresion-t09-ecpp/`). Resultado:
**1095 guiones; 1084 pasan, 9 fallan y 2 se omiten.**

- Pasan el fixture nuevo `pruebas-tanda-09-cpp-enum-convenciones/validar.py` y los tres guiones
  convertidos (`regcall/validar-negativo.py`, `regcall-dos-records/validar.py`,
  `c-enum-convenciones/validar.py`).
- Fallan los dos de Clang `cpp-variadica-preserve-none`, por la causa ya documentada, y en la
  corrida paralela `cpp-tamano-objeto/validar-indice-expresion`, que pasa con un solo proceso
  (`logs/regresion-t09-ecpp-reintento/`).
- Fallaban seis guiones `cpp-variadica-sysv-enum-*` (punteros `sysv_abi` variádicos con enums
  C++ con ámbito): el cambio hacía que esa ruta dejara de elegir su plan. Corregido en el mismo
  cambio (`variadicPlanArgument`, ver el registro); con el compilador corregido pasan los seis
  (`logs/regresion-t09-ecpp-fallos/`), y el subconjunto `pruebas-tanda-09-*variadic*`
  (`logs/regresion-t09-ecpp-variadica/`) da 187 de 191, con solo los dos `preserve-none`
  conocidos. El fixture de enums C++ vuelve a pasar
  (`A:/temp/nsc-t09-cpp-enum-convenciones-aw37adkl`).

Con el compilador corregido (`lote-ct2.cmd`): pasan las nueve CTests y
`nsc_cpp_c_identity_tests`; `ninja -k 0` solo falla por los tres símbolos ya conocidos.

La tanda 9 **sigue abierta**. T09-003 conserva estas capacidades sin evidencia:

- la unión C solo `float` con `regcall` (Clang la expande como `float`);
- records C++ por valor en punteros `regcall`;
- enums C++ con espacio de nombres en alias de puntero consumidos desde plantilla precompilada
  (también con `cdecl`);
- enums de base `wchar_t` con `swiftcall` y `swiftasynccall`;
- en el replay precompilado: `_Alignas` y otros atributos de campo (el catálogo no los guarda)
  y records empaquetados en punteros y variádicas.

## Regresión completa con enums C++ en espacios de nombres (2026-11-14)

El compilador incluye los cambios de
[registro-commits/t09-enum-cpp-espacios-nombres.md](registro-commits/t09-enum-cpp-espacios-nombres.md)
sobre `f50ed325f` (`lote-t09-ens.cmd`, registros en `logs/regresion-t09-ens/`). Resultado:
**1095 guiones; 1089 pasan, 4 fallan y 2 se omiten.**

- Pasa `pruebas-tanda-09-cpp-enum-convenciones/validar.py` sin `--espacios`. Con `--espacios`, en
  estática y DLL, pasa en las corridas del registro.
- Fallan los dos de Clang `cpp-variadica-preserve-none`, por la causa ya documentada.
- Fallan en la corrida paralela `cpp-tamano-objeto/validar-indice-llamada` y
  `cpp-tamano-objeto/validar-puntero-desplazado`. Con un solo proceso pasan los dos
  (`logs/regresion-t09-ens-reintento/`).

Con el mismo compilador (`lote-ct2.cmd`): pasan las nueve CTests y `nsc_cpp_c_identity_tests`;
`ninja -k 0` solo falla por los tres símbolos ya conocidos.

La tanda 9 **sigue abierta**. T09-003 conserva estas capacidades sin evidencia:

- la unión C solo `float` con `regcall` (Clang la expande como `float`);
- records C++ por valor en punteros `regcall`;
- enums de base `wchar_t` con `swiftcall` y `swiftasynccall`;
- en el replay precompilado: `_Alignas` y otros atributos de campo (el catálogo no los guarda)
  y records empaquetados en punteros y variádicas.

## Subconjunto con enums `wchar_t` y convenciones Swift (2026-11-14)

El compilador incluye el cambio de
[registro-commits/t09-enum-wchar-swift.md](registro-commits/t09-enum-wchar-swift.md) sobre
`b93d1cf09`. El cambio solo toca la rama del enum de `regCallFloatRecordScalarMatches`, que usan
los validadores de `regcall`, `preserve_none` y de las convenciones planificadas. Se repiten esos
guiones (`lote-subconjunto.cmd wide`, registros en `logs/regresion-t09-wide-*/`):

| Patrón | Guiones | Pasan | Fallan |
|---|---|---|---|
| `*regcall*` | 294 | 294 | 0 |
| `*enum*` | 59 | 59 | 0 |
| `*convenciones*` | 6 | 6 | 0 |
| `*preserve-none*` | 120 | 118 | 2 |
| `*planificad*` | 1 | 1 | 0 |

Fallan solo los dos de Clang `cpp-variadica-preserve-none`, por la causa ya documentada.

La tanda 9 **sigue abierta**. T09-003 conserva estas capacidades sin evidencia:

- la unión C solo `float` con `regcall` (Clang la expande como `float`);
- records C++ por valor en punteros `regcall`;
- en el replay precompilado: `_Alignas` y otros atributos de campo (el catálogo no los guarda)
  y records empaquetados en punteros y variádicas.

## Regresión completa con agregados homogéneos `regcall` (2026-11-14)

El compilador incluye los cambios de
[registro-commits/t09-regcall-agregados-homogeneos.md](registro-commits/t09-regcall-agregados-homogeneos.md)
sobre `15853aa75` (`lote-t09-hfa.cmd`, registros en `logs/regresion-t09-hfa/`). Resultado:
**1096 guiones; 1091 pasan, 3 fallan y 2 se omiten.**

- Pasan el fixture nuevo `pruebas-tanda-09-regcall-hfa-uniones/validar.py` y los cuatro guiones
  convertidos por `transformar-union.py`.
- Falla `c-union-convenciones/validar.py`: la corrida usó todavía su versión anterior, con la
  unión solo `float` como control negativo. Convertido en el mismo cambio, pasa en estática y DLL
  con `--precompilado` (`logs/fx-union-est.log`, `logs/fx-union-dll.log`).
- Fallan los dos de Clang `cpp-variadica-preserve-none`, por la causa ya documentada.

Con el mismo compilador (`lote-ct2.cmd`): pasan las nueve CTests y `nsc_cpp_c_identity_tests`;
`ninja -k 0` solo falla por los tres símbolos ya conocidos.

La tanda 9 **sigue abierta**. T09-003 conserva estas capacidades sin evidencia:

- records C++ por valor en punteros `regcall` (pasados o devueltos como un entero);
- en el replay precompilado: `_Alignas` y otros atributos de campo (el catálogo no los guarda)
  y records empaquetados en punteros y variádicas.

## Regresión completa con records C++ en punteros `regcall` (2026-11-14)

El compilador incluye los cambios de
[registro-commits/t09-cpp-regcall-puntero-records.md](registro-commits/t09-cpp-regcall-puntero-records.md)
sobre `de7af8353` (`lote-t09-cpr.cmd`, registros en `logs/regresion-t09-cpr/`). Resultado:
**1097 guiones; 1092 pasan, 3 fallan y 2 se omiten.**

- Pasan el fixture nuevo `pruebas-tanda-09-cpp-regcall-puntero-records/validar.py`,
  `pruebas-tanda-09-c-union-convenciones/validar.py` (ya convertido) y el fixture de enums C++, que
  ahora prueba por puntero `regcall` las firmas con records.
- Fallan los dos de Clang `cpp-variadica-preserve-none`, por la causa ya documentada.
- Falla en la corrida paralela `cpp-tamano-objeto/validar-indice-expresion`. Con un solo proceso
  pasa (`logs/regresion-t09-cpr-reintento/`).

Con el mismo compilador (`lote-ct2.cmd`): pasan las nueve CTests y `nsc_cpp_c_identity_tests`;
`ninja -k 0` solo falla por los tres símbolos ya conocidos.

La tanda 9 **sigue abierta**. T09-003 conserva estas capacidades sin evidencia, ambas del replay
precompilado:

- `_Alignas` y otros atributos de campo (el catálogo no los guarda);
- records empaquetados en punteros y variádicas.

## Regresión completa con records empaquetados y campos `char` en el replay (2026-11-14)

El compilador incluye los cambios de
[registro-commits/t09-replay-empaquetado-char.md](registro-commits/t09-replay-empaquetado-char.md)
sobre `0d2dc0b39` (`lote-t09-pk.cmd`, registros en `logs/regresion-t09-pk/`). Resultado:
**1098 guiones; 1094 pasan, 2 fallan y 2 se omiten.**

- Pasa el fixture nuevo `pruebas-tanda-09-precompilado-empaquetado/validar.py`.
- Fallan los dos de Clang `cpp-variadica-preserve-none`, por la causa ya documentada.

Con el mismo compilador (`lote-ct2.cmd`): pasan las nueve CTests y `nsc_cpp_c_identity_tests`;
`ninja -k 0` solo falla por los tres símbolos ya conocidos.

La tanda 9 **sigue abierta**. T09-003 conserva sin evidencia, en el replay precompilado, `_Alignas`
y otros atributos de campo (el catálogo no los guarda).
