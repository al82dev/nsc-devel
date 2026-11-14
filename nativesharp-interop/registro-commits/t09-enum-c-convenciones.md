# T09-003: enums C por valor con convenciones no `cdecl`

## Defecto

Una sonda (`build-claude-t08/tools/sonda-enum-cc.py`, evidencia
`A:/temp/nsc-t09-sonda-enum-9ryxaaha`) confirmó lo que ya registraba
[t09-regcall-sysv-records-plan.md](t09-regcall-sysv-records-plan.md). Un enum C por valor
solo se aceptaba con `cdecl` y `vectorcall`. Con las demás convenciones C se rechazaba en
llamada directa y por puntero:

| Convención | Diagnóstico |
|---|---|
| `regcall` | `regcall currently supports only int scalar signatures …` |
| `sysv_abi` | `sysv_abi currently supports only selected scalar, pointer and record signatures` |
| `preserve_most`, `preserve_all` | `… does not yet support this signature` |
| `preserve_none` | `preserve_none does not yet support this signature` |

Por puntero, cada una devolvía su `function pointer signature is not yet supported`.

Clang clasifica un enum por valor como su entero subyacente: el IR de Clang es idéntico
para `T09U8` y para `unsigned char`, incluidas las extensiones `zeroext`/`signext` de
`sysv_abi`.

Causa en el código. Las compuertas de convención comparaban claves NativeSharp con una
lista de enteros incorporados, y el validador compartido de planes
(`regCallFloatRecordScalarMatches`) no conocía `CppTypeKind::Enumeration`. En concreto:

- la HIR (`HirBuildCoordinator.cpp`);
- el emisor de llamadas, declaraciones y punteros (`OopLlvmCalls.cpp`,
  `OopLlvmDeclarations.cpp`, `OopLlvmClosuresDelegatesEvents.cpp`);
- la extensión SysV (`CppBoolAbi.h`);
- la conciliación del plan de alias (`sameVariadicType`);
- el replay precompilado de punteros regcall/sysv_abi;
- el códec de planes SysV con records.

## Cambio

Un enum C se trata como su entero subyacente solo si su identidad y su layout están
conciliados.

- **Clave subyacente.** `CppImportedEnumNumeric.h` añade `cppCEnumAbiScalarKey`. Devuelve
  la clave del entero subyacente solo para un enum C importado (`CEnum`) completo, sin
  error diferido, cuya base sea uno de los ocho enteros de ancho fijo y cuyo layout
  coincida con ese entero. Los enums C++ no entran.
- **Validador de planes.** `regCallFloatRecordScalarMatches` (`CppRegCallAbiValidation.h`)
  acepta un enum con nombre e identidad de Clang (USR), sin calificar, cuya base entera
  tenga su mismo tamaño y alineación. Lo compara como esa base. Es el validador que usan
  `regcall`, `preserve_none`, las convenciones por plan y los punteros. El llamador sigue
  conciliando la identidad.
- **HIR.** Para cada parámetro y para el retorno, `cEnumAbiKey` sustituye la clave del
  enum por la de su entero. Lo hace solo si la USR del tipo de Clang coincide con el enum
  importado y si tamaño, alineación y signo de la base coinciden. Las compuertas de
  `preserve_*`, `regcall` y `sysv_abi` leen esas claves.
  - Con `regcall`, `preserve_*` o una convención por plan, un enum que no pase la
    conciliación rechaza la firma (`cEnumIdentitiesValid`). Así, un enum C++ por valor
    con `regcall` sigue rechazado.
  - En `sysv_abi` con records, el enum se comprueba como su entero, con las extensiones
    del plan, en parámetro y retorno.
- **Emisor.** Las rutas `regcall` por plan, en llamada directa y por puntero, aceptan el
  enum como su entero y el retorno enum, que antes desreferenciaba un layout nulo.
  Además:
  - la extensión SysV (`applySysVIntegerExtension`, `applySysVExtension`) extiende un enum
    estrecho como su base;
  - las compuertas de punteros (`emitExternCDelegateCall`) leen la clave subyacente;
  - el plan de alias `preserve_none` se compara con las claves subyacentes;
  - `recordArgument` ya no trata un enum C como record, así que el puntero `sysv_abi` C
    sigue su plan pieza por pieza.
- **Plan del alias en vivo.** `sameVariadicType` admite, solo para la conciliación del
  plan de un puntero C no variádico, enums con la misma identidad de Clang y la misma base.
- **Replay precompilado.**
  - Punteros `regcall`/`sysv_abi` con records: el enum del catálogo se reproduce como su
    entero subyacente (Clang lo clasifica igual) y el plan se valida contra el tipo enum
    del alias.
  - Punteros `preserve_none` C con enums y sin records por valor: usan el plan del
    catálogo, aceptado solo por el validador escalar y con la identidad del enum conciliada.
- **Códec** (`CppCRecordMetadataCodec.cpp`): el plan SysV con records y retorno escalar
  admite enums C de cualquier base fija, con la extensión de esa base. Se reconstruye al
  decodificar con las mismas reglas y viaja en el campo existente. El contrato NSMeta
  vigente no cambia.

No se modificó Clang ni NSLink.

## Evidencia

### Fixture nuevo

`pruebas-tanda-09-c-enum-convenciones/validar.py`, con y sin `--dll`, siempre con
`--precompilado`.

Convenciones: `regcall`, `sysv_abi`, `preserve_most`, `preserve_all`, `preserve_none`,
`swiftcall`, `swiftasynccall` e `intel_ocl_bicc`.

Enums:

- `T09Color`, base int;
- `T09Big`, base int con `INT_MIN`, un negativo e `INT_MAX`;
- `T09U8`, base `unsigned char`;
- `T09S16`, base `short` negativa;
- `T09U64`, base `unsigned long long` con el bit alto.

Firmas: parámetro, retorno, ida y vuelta, enums mezclados con `double` y con otro enum.
Con `regcall`, `sysv_abi` y `preserve_*` hay además un record `T09Pair` con un enum
estrecho en parámetro y otro con retorno enum estrecho.

Por cada convención, de O0 a O3, la prueba:

- ejecuta un control C compilado por Clang 23.1.0 y el consumidor NativeSharp con las
  mismas comprobaciones;
- exige, en cada declaración del IR de NSC, la convención LLVM y las extensiones
  `signext`/`zeroext` de cada parámetro y del retorno idénticas a la definición de Clang;
- exige esa convención en cada llamada directa;
- con las convenciones que NSC proyecta por puntero, exige la misma convención en todas
  las llamadas indirectas;
- consume una plantilla precompilada con y sin encabezado, con llamadas directas y por
  puntero, y comprueba el IR del consumidor.

El control negativo conserva un rechazo: `enum class` C++ por valor con `regcall`, que
Clang compila. Se comprueba el diagnóstico `regcall currently supports only int scalar
signatures`.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática + precompilado | pasa (8 convenciones × O0–O3) | `A:/temp/nsc-t09-c-enum-convenciones-ow_tsafa` |
| DLL + precompilado | pasa (8 convenciones × O0–O3) | `A:/temp/nsc-t09-c-enum-convenciones-5a4ojo5a` |

Sonda posterior con 9 convenciones × 8 firmas, en directa y por puntero
(`A:/temp/nsc-t09-sonda-enum-i6xgw6gp`): todas las celdas pasan, salvo los casos que
siguen fuera:

- los punteros `swift*` e `intel_ocl_bicc`, rechazados con el diagnóstico del alias;
- los records con esas tres convenciones.

### Controles negativos convertidos

El enum C con `regcall` era el control negativo de dos guiones y ahora se admite. Siguiendo
la política acordada, ambos lo comparan con Clang (O0–O3: control C, IR y ejecución) y
conservan el rechazo con un `enum class` C++ por valor:

- `pruebas-tanda-09-regcall/validar-negativo.py`;
- `pruebas-tanda-09-regcall-dos-records/validar.py`.

Resultados en la regresión de la tanda: ver
[TANDA-09-VALIDACION-AGRUPADA.md](../TANDA-09-VALIDACION-AGRUPADA.md).

## Límites

- Solo enums C con nombre (`CEnum`) y base de ancho fijo de 8, 16, 32 o 64 bits. Siguen
  rechazados con `regcall`, `preserve_*` y las convenciones por plan:
  - los enums C++ por valor;
  - los enums `const` o `volatile` por valor;
  - los enums con base `bool` o `char`.
- `swiftcall`, `swiftasynccall` e `intel_ocl_bicc` conservan sus límites: sin records por
  valor y sin punteros, porque NativeSharp no tiene esos tipos de función.
- Variádicas: no cambian. Un enum C como argumento variádico no se acredita aquí.
- En el replay precompilado, los punteros `preserve_none` C solo con escalares de tipos
  incorporados no cambian de ruta; únicamente los que llevan enums C usan el plan del
  catálogo.
