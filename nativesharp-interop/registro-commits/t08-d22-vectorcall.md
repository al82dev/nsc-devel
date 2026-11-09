# T08-D22: funciones y callbacks C `__vectorcall`

Fecha: 2026-11-09. Decisión del usuario: T08-D22, resolver `__vectorcall`; un diagnóstico no es solución final.
Casillas: T08-017-03d y T08-017-03d1.

## Problema

- Una función C `__vectorcall` no se importaba. El HIR la rechazaba con «calling convention 'vectorcall' is not yet
  supported».
- El backend ya tenía delegates `extern vectorcall` (ítems 344 y 345), pero clasificaba mal los agregados:
  - solo reconocía HVA de `Float32x4`;
  - no contaba registros.

  Clang (`WinX86_64ABIInfo`) también trata como agregado homogéneo un record de 1 a 4 `float` o `double` (HFA), y
  reparte seis registros XMM en dos pasadas.
- Un tipo de puntero a función C con un record por valor (`D2 (*)(D2, F2)`) no se proyectaba: «parameter 0 is not
  yet mapped». Eso impedía los callbacks HFA, y también los callbacks cdecl con records por valor.
- `VecFn v = plain_add;` se aceptaba sin comprobar la convención. Desde T08-D18, una función C es su propia
  dirección, así que el resultado era una llamada con el ABI equivocado.

## Sonda

`A:\nsc-interop-build\claude\sondas\vectorcall\abi.c` con `clang -S -emit-llvm` muestra cómo baja Clang cada firma
`__vectorcall`. Por ejemplo:

- `v_d2(D2, int)` pasa `%struct.D2 inreg`;
- `v_many(D2, D2, D2, double, double)` pasa los dos primeros `D2` `inreg` y el tercero por puntero, porque los
  `double` ocupan antes sus registros posicionales;
- `v_i2(I2)` (enteros) usa la regla Win64 ordinaria (`i64`);
- `v_mixed(Mixed)` (`double` y `float`) va por puntero;
- `r_d2` devuelve `%struct.D2` en registros;
- `r_big` usa `sret`.

## Cambio

**Importación** (`HirBuildCoordinator`, `ProjectOopHirFunction`/`ProjectOopMirFunction`, cachés):

- La función C `__vectorcall` se importa con `cppVectorCall`.
- La marca viaja del HIR al MIR y entra en las identidades y la serialización de las cachés HIR/MIR.

**Clasificación** (`OopLlvmTypesAbiAttributes.cpp`):

- `vectorCallAggregate`:
  - calcula el agregado homogéneo como Clang: base `float`, `double` o `Float32x4`, de 1 a 4 miembros, sin
    relleno ni solapamiento;
  - cuenta records anidados, unions (el miembro mayor) y arreglos fijos;
  - recorre el tipo de forma iterativa.
- `vectorCallPassing` aplica las dos pasadas de `computeVectorCallArgs`:
  - entre los seis primeros argumentos, cada `float`, `double` o vector toma un registro mientras queden;
  - después, cada agregado toma tantos registros como miembros tenga (`inreg`) o va por referencia a una copia;
  - una HFA de 8 bytes también va por referencia, no como `i64`.
- `vectorCallArgument` convierte el portador al valor físico.

**Llamadas y declaraciones**:

- `emitExternCDelegateCall`, el tipo de los thunks `__vectorcall_boundary` y sus atributos usan esa misma
  clasificación.
- El thunk lee la copia por referencia y devuelve una HFA de 8 bytes que NativeSharp devuelve directa.
- La declaración de la importación (`vectorCallImportType`) tiene el mismo tipo físico, la convención
  `x86_vectorcallcc` y los atributos `inreg`.
- La llamada directa (`lowerStaticCall`) pasa por `emitExternCDelegateCall` con los portadores de vectores y HVA
  sin envolver.

**Callbacks con records por valor** (`CppSemanticFunction.cpp`, `cCallbackRecordValue`):

- Un tipo de puntero a función C acepta un struct o union C por valor, en parámetros y retorno.
- Solo vale en contextos C (llamadas, campos y globales C) y con el mismo layout de valor que exige T08-D09.

**Convención** (`LambdaDelegateBinder.cpp`):

- Una función C convertida a un tipo de callback C debe tener la misma convención.
- cdecl y `__vectorcall` nunca se sustituyen: «C function 'plain_add' uses the 'cdecl' calling convention, which
  the callback type '…' does not use».

## Pruebas

### Nueva: `pruebas-tanda-08-callbacks-c/validar-vectorcall.py`

Aprobada en O0–O3, con los rechazos en O0.

**Caminos:**

- control Clang;
- directo con biblioteca estática y con DLL (`Runtime`, encabezado con `dllimport`);
- plantilla precompilada consumida con encabezado y sin él.

**Llamadas directas, por puntero `__vectorcall` y desde C hacia callbacks C y NativeSharp:**

- más de seis argumentos flotantes;
- HFA `D2`, `F4` y `D4` en registros, y devueltas en registros;
- el tercer `D2` de `v_many`, por referencia;
- una HFA de 8 bytes (`F2`) por referencia al agotarse los registros, y devuelta en registros;
- HFA con record anidado y con arreglo (`Nest`, `Arr`);
- records no homogéneos (`Mixed`, `Big`) y un retorno `Big` por `sret`.

**Identidad:**

- `same_vec(vec_add)`;
- `vec_address() == v`, también con DLL.

**Callbacks cdecl con records por valor:** `PlainD2Fn`.

**Rechazos:**

- cdecl → `VecFn`, como variable y como argumento;
- `__vectorcall` → `PlainFn`.

### Actualizadas (punto 6: fijaban el rechazo anterior)

- `pruebas-tanda-08-callbacks-c/validar-callbacks.py`: el caso 03d pasa de rechazo a caso positivo. Llama a
  `vec_add` directamente y por `VecFn`, y lo pasa a C.
- `tests/unit/CppClangSessionTests.cpp`: «unimplemented vectorcall is rejected before incorrect HIR emission»
  pasa a «vectorcall import compiles to HIR with its convention».

## Validación

`A:\nsc-interop-build\claude\scripts\regresion-d22.cmd`, resultado 0:

- `callbacks-c`: `validar-vectorcall`, `validar-callbacks` y `validar-identidad-funcion-c`;
- `arreglos-c\validar-arreglos`;
- `typedef-c`: `validar-record-dos-encabezados`, `validar-formas` y `validar-reconciliacion`.

ctest, con el entorno de las pruebas (`NSC_LINKER`, `NSC_WINDOWS_RUNTIME_LIBRARY`):

- pasan `nsc.cpp-interop.session`, `nsc.item343.callback-output-parameters` y `nsc.item345.vectorcall-hva`;
- `nsc.item344.vectorcall` pasa 6 de 7 comprobaciones. Las de ejecución, encabezado y degradación siguen bien. Falla
  «current NSMETA preserves the vectorcall delegate flag»: compara `formatMinor` con `kMetadataFormatMinor`, y el
  productor escribe un minor mínimo. Es T08-D24, la misma causa que en `nsc_namespace_function_metadata_tests`;
  el indicador `vectorcall` del delegate sí se conserva.
