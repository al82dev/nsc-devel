# T09-003: records C regcall sin expansión y punteros C sysv_abi con records, por plan de Clang

## Defectos

### regcall

Clang 23.1.0 pasa un record C regcall de 1, 2, 4 u 8 bytes como un entero directo, y uno
mayor de forma indirecta; los mezcla con escalares sin expandirlo. Ejemplo de la matriz de
firmas: `double(float, int*, R8, double)` → `(float, ptr, i64, double)`.

NSC rechazaba esas firmas, en llamada directa y por puntero:

- con `regcall currently supports only int scalar signatures ...`;
- y con `regcall function pointer signature is not yet supported`.

Causa confirmada en el código:

- Para un plan sin expansión, los validadores de regcall solo admitían retorno `double`,
  `int` o `void`, y exigían al menos un argumento indirecto.
- El emisor directo, el de punteros y el de declaraciones ya bajaban esos argumentos a partir
  del plan: un record directo se carga como entero de su tamaño y uno indirecto se pasa por
  puntero a una copia.
- Faltaba, por tanto, la puerta de aceptación.

### sysv_abi

En los punteros a función C `sysv_abi`, el emisor solo aceptaba algunas formas fijas:

- el record pequeño de ida y vuelta, que además exigía un record C++;
- dos enteros;
- un retorno indirecto de 20 bytes.

Por eso la matriz rechazaba cinco celdas:

- record de 8 bytes (`sysv_abi C++ function pointer record provenance is invalid`);
- retornos de 36 bytes, 8 bytes flotantes y 16 bytes mixtos;
- la firma mixta.

La reconstrucción precompilada del alias C (réplica en una sesión Clang) solo admitía records
de 12, 16 y 20 bytes de enteros o `{double, int}`, y escalares `int`.

## Cambio

### Validadores compartidos (`CppRegCallAbiValidation.h`)

- `isRegCallWin64NonExpandedRecordsReturningScalar`: plan sin expansión con retorno escalar o
  `void` de cualquier tipo admitido. El plan por sí solo no distingue un entero de un record
  pasado como entero, así que quien llama indica si la firma fuente tiene un record por valor.
- `isRegCallWin64NonExpandedRecordsReturningRecord`: retorno de record compacto, como entero
  de su tamaño, o indirecto (`sret`).
- `isRegCallWin64RecordsWithScalarReturn`, `isRegCallWin64RecordsWithRecordReturn` e
  `isRegCallWin64NonExpandedRecordsPlan`: formas comprobadas contra la firma fuente.
  - Exigen identidad, USR y layout del record de retorno.
  - Concilian cada argumento con el plan mediante `regCallFloatRecordArgumentsMatch`, con
    records directos o indirectos.
- `regCallPhysicalReturnMatchesKey`: comprueba que el retorno físico corresponde al tipo
  NativeSharp.

### regcall

- `HirBuildCoordinator.cpp`:
  - La puerta regcall acepta `regCallPlainRecords` solo si el validador fuente acepta el plan.
  - Cada record por parámetro debe pasar `regCallAdditionalRecordEligible`: identidad, USR,
    layout y elegibilidad trivial C++.
  - El record de retorno se concilia con su tipo importado.
- `OopLlvmCalls.cpp`, `OopLlvmDeclarations.cpp` y `OopLlvmClosuresDelegatesEvents.cpp`:
  - Las ramas regcall existentes, guiadas por plan, también se activan con esos planes.
  - El retorno indirecto se reconoce también sin expansión.
- `CppCRecordMetadataCodec.cpp`: los planes de función y de alias regcall de este tipo viajan
  en el campo ya existente del catálogo C y se revalidan al decodificar. El contrato NSMeta
  vigente no cambia.
- `CppProjectSessions.cpp`: la réplica precompilada de un alias regcall acepta el mismo
  validador.

### sysv_abi

- `OopLlvmClosuresDelegatesEvents.cpp`: un puntero `sysv_abi` no variádico cuyos records por
  valor son todos `struct` C se baja pieza por pieza desde el plan de Clang.
  - Cada record se concilia con su layout importado (`CStruct`, tamaño y alineación).
  - Cada pieza directa debe caber dentro del record, en el offset natural de la estructura de
    coerción.
  - Un record indirecto se pasa `byval` con la alineación del plan.
  - Un retorno indirecto usa `sret`.
  - Un retorno directo se reconstruye desde sus piezas.
  - Cualquier otra forma se rechaza con diagnóstico; las rutas C++ no cambian.
- `CppProjectSessions.cpp`: la réplica precompilada de un alias C `sysv_abi` acepta, como la de
  regcall, cualquier record C con sintaxis de réplica simple y los escalares admitidos. La
  réplica se compara campo a campo con el catálogo antes de pedir el plan a Clang.

No se modificó Clang ni NSLink.

## Evidencia

- `pruebas-tanda-09-regcall-c-record-mixto/validar.py --precompilado` (nuevo) cubre:
  - las firmas C regcall `double(float, int*, T09Pair, double)`, `int(T09Small, T09Wide, int)`,
    `void(int*, T09Pair)`, `long long(T09Wide, long long)`, `unsigned char(T09Small)`,
    `T09Pair(float, T09Small)` (retorno compacto) y `T09Wide(T09Pair, int)` (retorno indirecto);
  - cuatro punteros a función con esas firmas.

  La prueba compara contra Clang 23.1.0 en estática y DLL O0–O3, directa y por puntero. También
  usa una plantilla precompilada, con y sin encabezado. Exige `x86_regcallcc` y el símbolo
  `__regcall3__` en el IR del consumidor directo y del precompilado. Evidencia:
  `A:/temp/nsc-t09-regcall-c-record-mixto-xd6dzw5x` (estática) y
  `A:/temp/nsc-t09-regcall-c-record-mixto-wii5n9al` (DLL).
- `pruebas-tanda-09-sysv-c-puntero-records/validar.py --precompilado` (nuevo) cubre:
  - punteros C `sysv_abi` con records de 8 bytes (`i64`), 12 bytes (`{i64, i32}`), 36 bytes
    (`sret` y `byval`), 8 bytes flotantes (`<2 x float>`) y 16 bytes mixtos (`{double, i32}`);
  - una firma mixta y una `void` con record `byval` y record flotante.

  Las mismas condiciones; el IR debe contener `x86_64_sysvcc` con cada tipo físico. Evidencia:
  `A:/temp/nsc-t09-sysv-c-puntero-records-y0yffrcy` (estática) y
  `A:/temp/nsc-t09-sysv-c-puntero-records-kpdir8u1` (DLL).
- Matriz de firmas por convención: pasan 252 de 252 celdas, sin rechazos ni fallos
  (`A:/temp/nsc-t09-matriz-firmas-dy156dcf`).
  - Una corrida intermedia marcó cinco celdas SysV como "convención indirecta `ccc`". Se debía a
    que el nombre del valor LLVM (`%sysv.cptr.call = call ...`) hacía coincidir la expresión
    regular de la prueba.
  - El valor se renombró en el emisor; la prueba no se cambió.
- Regresión completa de los fixtures de la tanda 9 (`build-claude-t08/tools/regresion-t09.py`,
  1089 scripts `validar*.py` en modo estático):
  - 1066 pasaron, 21 fallaron y 2 se omitieron (requieren `--caso --mutador`).
  - Los fallos se analizan en la sección siguiente. El resumen está en
    `build-claude-t08/logs/regresion-t09/resumen.txt`.
- Pasan los CTest `clang-abi-probe`, `regcall-record-plan`, `variadic-pointer`,
  `project-loader` e `item331`, `item332`, `item333`, `item344` e `item345`, y la prueba de
  identidad C/C++.

  La construcción completa con `ninja -k 0` enlaza `nsc` y las pruebas usadas. Unos 80
  ejecutables de prueba ajenos a T09 no enlazan. Les faltan símbolos de
  `PrecompiledSemanticSnapshotCodec::decode`, `cppCInlineLinkSymbol` y
  `projectCppCxxEnumConstant`: es una dependencia de bibliotecas de esos objetivos en CMake que
  este cambio no toca.

## Controles negativos existentes que ya no rechazaban

La regresión de los fixtures regcall mostró dos controles negativos que fallaban sin estos
cambios. Se comprobó guardando los cambios con `git stash`, reconstruyendo `nsc` y repitiendo:

- `pruebas-tanda-09-regcall/validar-negativo.py`;
- el caso `forma-abi-distinta` de `pruebas-tanda-09-regcall-dos-records/validar.py`.

Evidencia: `A:/temp/nsc-t09-regcall-negativo-8_b9zg6a` y `A:/temp/nsc-t09-two-records-rf2_t37c`.

Sus firmas ya se admiten por rutas anteriores, con expansión de records flotantes. Siguiendo
el criterio aplicado al control SysV:

- cada uno pasa a una comparación positiva contra Clang;
- se añade un control negativo separado con una firma que NSC sigue sin admitir.

Conversión aplicada:

- `pruebas-tanda-09-regcall/validar-negativo.py`:
  - El record `{float x, y, z}` (tres `float` expandidos) se compara con un control C en
    O0–O3 y exige la llamada regcall de Clang en el IR.
  - El rechazo `regcall currently supports only int scalar signatures` se conserva con un
    enum C por valor (`A:/temp/nsc-t09-regcall-negativo-2vw5ezvo`).
- `pruebas-tanda-09-regcall-dos-records/validar.py`:
  - `Triple combine(Triple, Other, int)` recibe una implementación propia, un control C y una
    ejecución NativeSharp en O0–O3.
  - El rechazo se conserva con un enum C por valor (`A:/temp/nsc-t09-two-records-b2wxf9gk`).

La regresión completa también mostró controles que el cambio SysV de este registro vuelve
obsoletos:

- `pruebas-tanda-09-sysv-indirecta/validar.py`:
  - Rechazaba el record C de 24 bytes `byval` por un puntero `sysv_abi`.
  - Ahora lo compila con una implementación propia, lo compara con un control C y exige
    `byval ... align 8` en el IR.
  - El rechazo `byval plan is not supported` se conserva, sin cambiar su texto, con una
    `union` C del mismo tamaño (`A:/temp/nsc-t09-sysv-indirecta-ge3c5uzs`).
- Los 14 `validar-precompilado.py` de `pruebas-tanda-09-sysv-indirecta-*`:
  - Rechazaban una variante de signo del record (`int` → `unsigned int`), que el replay
    anterior no reconstruía.
  - Ahora compilan la biblioteca de esa variante y ejecutan el consumidor precompilado sin
    encabezado.
  - El rechazo `sysv_abi function pointer signature is not yet supported` se conserva con el
    mismo campo calificado `volatile`, que el replay no reconstruye.
  - Ejemplos: `A:/temp/nsc-t09-sysv-indirecta-record-pre-opg7bppf` y
    `A:/temp/nsc-t09-sysv-indirecta-record-pre-vdgvc4c_`; los 14 pasan
    (`build-claude-t08/logs/aplicar-neg.log`).

También se encontraron dos controles anteriores que fallaban por razones ajenas a este cambio:

- `pruebas-tanda-09-sysv-indirecta-variadica-record/validar-transporte.py` esperaba una
  barrera LLVM. El emisor variádico ya la había superado antes de este cambio: `validar.py`,
  del mismo fixture, compara la ejecución. Ahora exige las piezas de Clang en el IR
  (`A:/temp/nsc-t09-sysv-indirecta-variadica-record-g34rflu0`).
- `pruebas-tanda-09-preserve-none-record/validar-rechazo.py` esperaba el rechazo de
  `Small(Small, int)` C++ de 44 bytes, que ya pasaba por la ruta C++ con plan.
  - Ahora ejecuta esa firma, directa y por puntero.
  - El rechazo se conserva con una firma C++ `preserve_none` de 33 parámetros.

`pruebas-tanda-09-cpp-variadica-preserve-none` sigue fallando en el control compilado por
Clang, antes de llegar a NSC. Corresponde a la observación abierta
[t09-preserve-none-variadica-observacion.md](t09-preserve-none-variadica-observacion.md):
Clang 23.1.0 inicializa en Windows x64 un `va_list` `preserve_none` que no reserva. NSC
rechaza esas variádicas al importarlas.

## Límites

- regcall:
  - Se acreditan los records C `struct` de 4, 8 y 36 bytes, en las formas y los retornos
    probados.
  - No se acreditan `union` por valor ni records con campos expandidos mixtos
    flotante/entero fuera de las rutas anteriores.
  - Un enum C por valor con regcall sigue rechazado: el control negativo lo deja explícito.
- sysv_abi por puntero:
  - Solo los records C `struct`. Una `union` por valor se rechaza.
  - Los records C++ conservan sus rutas anteriores.
  - El replay precompilado no reconstruye campos `volatile`, `const` ni `_Alignas`; esas firmas
    se rechazan.
- Una sonda ajena a los fixtures (`A:/temp/nsc-t09-sonda-enum-t_spexgt`) muestra que un enum
  C por valor solo se acepta con `cdecl` y `vectorcall`. Con `regcall`, `sysv_abi` y
  `preserve_*` se rechaza en llamada directa y por puntero. No hay aceptaciones con otra
  convención.
