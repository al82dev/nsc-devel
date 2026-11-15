# T09-003: enums C++ por valor con convenciones no `cdecl`

## Defecto

Sonda `build-claude-t08/tools/sonda-enum-cpp.py`, evidencia
`A:/temp/nsc-t09-sonda-enum-cpp-2ekm3vqb`. Funciones C++ que reciben y devuelven un enum por
valor: `enum class` de base `int`, `unsigned char`, `short` y `unsigned long long`, y un enum
sin ámbito. En llamada directa y por puntero:

| Convención | Resultado |
|---|---|
| `cdecl`, `vectorcall` | pasan |
| `regcall` | `regcall currently supports only int scalar signatures …` |
| `sysv_abi` | `sysv_abi currently supports only selected scalar, pointer and record signatures …` |
| `preserve_most`, `preserve_all` | `… does not yet support this signature …` |
| `preserve_none` | `preserve_none does not yet support this signature …` |

Por puntero, cada una devolvía su `function pointer signature is not yet supported`. Clang
compila todos los casos y clasifica el enum como su entero subyacente, igual que en C.

Causa en el código. El cambio de los enums C
([t09-enum-c-convenciones.md](t09-enum-c-convenciones.md)) limitó la clave ABI del enum
(`cppCEnumAbiScalarKey`) a los enums C importados (`CEnum`). Los enums C++ (`CppEnum`) tienen
la misma base verificada (`enumBase`) y el mismo layout, pero las compuertas de convención los
seguían comparando como tipos. Además, dos rutas de punteros C++ `sysv_abi` con records solo
conocían escalares incorporados:

- los validadores `isSysVCppRecordScalarArgument` e `isSysVCppRecordPointerScalarReturn`
  (`CppSysVRecordAbiValidation.h`);
- la bajada de esos punteros (`OopLlvmClosuresDelegatesEvents.cpp`) y la selección de ruta del
  plan precompilado de alias C++ (`planPrecompiledCppFunctionPointerForAlias`), que no
  reconocían un retorno enum.

## Cambio

- `cppCEnumAbiScalarKey` pasa a `cppEnumAbiScalarKey` y acepta un enum importado C o C++
  (`CEnum` o `CppEnum`), completo, sin error diferido, cuya base sea uno de los ocho enteros de
  ancho fijo y cuyo layout coincida con ese entero. Las reglas aritméticas de C
  (`cppImportedCEnumUnderlying`, `cppCEnumMixedUintInt`) no cambian. El renombre alcanza los
  usos existentes en la HIR, el emisor, `CppBoolAbi.h` y el replay precompilado; las
  comprobaciones que los rodean (identidad USR, tamaño, alineación y signo) no cambian.
- `sysVCppEnumAsInteger` (`CppSysVRecordAbiValidation.h`) presenta un enum con nombre e
  identidad de Clang como su entero subyacente cuando este tiene su tamaño y alineación. Lo usan
  los dos validadores de punteros C++ `sysv_abi` con records.
- En la bajada de esos punteros, el retorno y los argumentos escalares se leen con la clave del
  entero subyacente (`cppScalarReturnKey`, `scalarKey`), también para las extensiones.
- `planPrecompiledCppFunctionPointerForAlias` reconoce un retorno enum como escalar en los
  punteros C++ `sysv_abi` y `preserve_none`; los validadores del plan siguen comprobando el
  enum contra el plan de Clang.

- La regresión completa mostró que las llamadas por puntero `sysv_abi` variádicas con enums C++
  con ámbito dejaban de elegir su plan: esa ruta reconocía el enum C++ como argumento especial
  porque la clave ABI no lo convertía. `variadicPlanArgument` conserva esa selección solo en la
  compuerta de esa ruta, que sigue comprobando cada enum contra el plan; las demás rutas lo leen
  como su entero.

No se modificó Clang ni NSLink. Los planes viajan en los campos existentes y el contrato
NSMeta vigente no cambia.

## Evidencia

### Sondas tras el cambio

- `A:/temp/nsc-t09-sonda-enum-cpp-u88h3hcw`: todas las celdas pasan en directa y por puntero
  con `regcall`, `sysv_abi`, `preserve_most` y `preserve_none`, y en directa con
  `preserve_all`. Los punteros `preserve_all` no se prueban en C++: Clang no sabe decorar esa
  convención (`cannot mangle this calling convention yet`), así que el fixture la declara
  `extern "C"`.
- `sonda-ecpp-ptr.py` prueba cada firma del fixture por puntero en sesión viva: tras el cambio
  pasan todas con `sysv_abi`, `preserve_most` y `preserve_none`.

### Fixture nuevo

`pruebas-tanda-09-cpp-enum-convenciones/validar.py`, con y sin `--dll`, siempre con
`--precompilado`. Convenciones: `regcall`, `sysv_abi`, `preserve_most`, `preserve_none`,
`preserve_all` (`extern "C"`), `swiftcall` y `swiftasynccall`.

Enums: `enum class` de base `int`, `unsigned char`, `short`, `unsigned long long` y `wchar_t`
(con `swift*`, desde [t09-enum-wchar-swift.md](t09-enum-wchar-swift.md)), y un enum sin ámbito con `INT_MIN` e `INT_MAX`. Firmas: parámetro, retorno, ida y vuelta,
mezclados con `double` y con otro enum; con `regcall`, `sysv_abi`, `preserve_most` y
`preserve_none`, además un record C++ en parámetro y otro con retorno enum estrecho.

De O0 a O3, la prueba:

- ejecuta un control C++ compilado por Clang 23.1.0 y el consumidor NativeSharp con las mismas
  comprobaciones;
- exige en cada declaración del IR de NSC la convención y las extensiones `signext`/`zeroext`
  de cada parámetro y del retorno idénticas a la definición de Clang, y esa convención en cada
  llamada directa (`call` o `invoke`);
- exige la convención en las llamadas por puntero (con `regcall`, solo las firmas sin record);
- consume una plantilla precompilada con y sin encabezado, con llamadas directas y por puntero.

El control negativo comprueba que una unión C solo de `float` con `regcall`, que Clang expande
como `float`, sigue rechazada con `regcall currently supports only int scalar signatures`.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática + precompilado | pasa (7 convenciones × O0–O3) | `A:/temp/nsc-t09-cpp-enum-convenciones-a_65o_e0` |
| DLL + precompilado | pasa (7 convenciones × O0–O3) | `A:/temp/nsc-t09-cpp-enum-convenciones-gkyucxx6` |

### Controles negativos convertidos

El `enum class` C++ con `regcall` era el control negativo de tres guiones y ahora se admite.
Siguiendo la política acordada, cada guion lo compara con Clang (el callee compilado por
Clang da el resultado esperado de O0 a O3 y NSC lo llama con `x86_regcallcc`) y conserva el
rechazo con la unión C solo de `float`, con el mismo diagnóstico:

- `pruebas-tanda-09-regcall/validar-negativo.py`;
- `pruebas-tanda-09-regcall-dos-records/validar.py`;
- `pruebas-tanda-09-c-enum-convenciones/validar.py`.

Pasan los tres (`A:/temp/nsc-t09-regcall-negativo-as51c8vl`,
`A:/temp/nsc-t09-two-records-6g0n8a29`, `A:/temp/nsc-t09-c-enum-convenciones-uqh51x_h` estática y
`A:/temp/nsc-t09-c-enum-convenciones-fithoavu` DLL, ambas con `--precompilado`).

El enum C++ de base `wchar_t`, que se consideró como nuevo rechazo, también se proyecta
(`A:/temp/nsc-t09-cpp-enum-convenciones-btbw61oe`); por eso pasa a ser un caso positivo del
fixture.

Regresión de la tanda: ver [TANDA-09-VALIDACION-AGRUPADA.md](../TANDA-09-VALIDACION-AGRUPADA.md).

## Límites

- Los records C++ por valor en punteros `regcall` siguen rechazados, con o sin enums
  (`sonda-ecpp-ptr.py`, `A:/temp/nsc-t09-sonda-ecpp-ptr-ug4xtu7i`). Es un límite previo.
- Un enum C++ declarado dentro de un espacio de nombres, usado en la firma de un alias de
  puntero, no se reconstruía en un consumidor precompilado, con ninguna convención (también
  `cdecl`): `C callback signature contains an unavailable type`
  (`A:/temp/nsc-t09-sonda-ecpp-pre-r2ngfp2e`). Resuelto después; ver
  [t09-enum-cpp-espacios-nombres.md](t09-enum-cpp-espacios-nombres.md). El fixture lo prueba con
  `--espacios`.
- Un enum de base `wchar_t` se rechazaba con `swiftcall` y `swiftasynccall`
  (`C/C++ calling convention 'swiftcall' is not yet supported in HIR`). Resuelto después; ver
  [t09-enum-wchar-swift.md](t09-enum-wchar-swift.md).
- Los punteros `swiftcall`, `swiftasynccall` e `intel_ocl_bicc` siguen sin tipo de función
  NativeSharp. `intel_ocl_bicc` no se usa en C++.
