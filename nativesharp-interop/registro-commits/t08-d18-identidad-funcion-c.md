# T08-D18: dirección de una función C sin el envoltorio `__c_boundary`

Fecha: 2026-11-09. Casillas: T08-D18 y T08-017-04a1
([decisión anterior](t08-callbacks-c.md#decisión-pendiente-t08-017-04a1)).

## Problema

Una función C convertida desde NativeSharp a un puntero a función C (`BinOp op = mul;`, `s.op = mul;`) se guardaba
como el envoltorio `mul__c_boundary`, un `invoke` con `__C_specific_handler`. La llamada funcionaba, pero la
dirección no era la de `mul`, así que en C `s->op == mul` daba falso. Tampoco coincidía con el puntero que devuelve
C.

## Causa raíz

`lowerDelegateCreate` (`OopLlvmClosuresDelegatesEvents.cpp`) entregaba a C siempre el thunk de
`callbackGuardThunk`, también cuando el destino era una declaración C sin cuerpo NativeSharp. El thunk existe para
que una excepción NativeSharp no se propague por marcos ajenos, y una función C no puede lanzarla.
`defineCallbackGuardThunks` (`OopLlvmThunksAndWrappers.cpp`) definía el thunk de antemano.

## Decisión aplicada

Se aplica la opción 1: usar directamente el símbolo C cuando la función es C.

- **Función C sin cuerpo NativeSharp** (`isImportedC` o `isCppExternal`, de un encabezado, de un `extern` o de un
  catálogo precompilado): el puntero es su propia dirección, como en C. Con una DLL, LLVM la toma de la IAT, así
  que es la dirección real de la función en la DLL.
- **Alias TLS de DLL homónimas, enlazados en ejecución:** el puntero es la dirección enlazada en ejecución,
  obtenida con el mismo ayudante que ya usaban las funciones variádicas.
- **Método o lambda NativeSharp:** conserva la frontera. Su dirección es la del thunk, y sigue atrapando una
  excepción NativeSharp.

## Cambio

- `src/LLVM/Internal/OopLlvmDirectCFunction.h` (nuevo): `directCFunctionAddress`.
- `OopLlvmClosuresDelegatesEvents.cpp`, `lowerDelegateCreate`:
  - devuelve la función C declarada;
  - para un alias TLS, devuelve la dirección enlazada en ejecución.
- `OopLlvmThunksAndWrappers.cpp`, `defineCallbackGuardThunks`: no define thunks para esos destinos.

NSMeta no cambia.

## Pruebas

**Nueva:** `pruebas-tanda-08-callbacks-c/validar-identidad-funcion-c.py`. Aprobada en O0–O3, con un control Clang.

- **Biblioteca C estática y DLL:**
  - en C, `is_mul(op)` y `ops_are_c(&s)` comparan con `mul` y `add`;
  - en NativeSharp, `op == c_mul()` y `s.op == c_mul()`;
  - dos conversiones de `mul` dan el mismo puntero;
  - llamadas por el puntero desde C y desde NativeSharp.
- **Método NativeSharp `NativeMul`:** convertido al mismo tipo, no es igual a `mul` y C lo llama correctamente.
- Cada caso se ejecuta directo y desde una plantilla precompilada consumida sin el encabezado.
- **IR:** no hay `mul__c_boundary` ni `add__c_boundary`, y `NativeMul` conserva su `__c_boundary`.

## Regresión

`A:\nsc-interop-build\claude\scripts\regresion-d18.cmd`, resultado 0:

- la suite nueva y `callbacks-c/validar-callbacks.py`;
- `tls-dll-distintas`: variádicas (normal, puntero y agregados), transitivas homónimas y dos orígenes;
- los validadores `callback` (normal y record precompilado) y `bool-callback-c`;
- la batería `identidad-c\validar.cmd`;
- `pruebas-proyecto/ejecutar-dll-c.cmd`;
- el conjunto de D02.
