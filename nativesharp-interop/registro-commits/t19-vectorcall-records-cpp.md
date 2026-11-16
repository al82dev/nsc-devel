# Tanda 19: records C++ por valor con `__vectorcall` y retorno en memoria precompilado (T19-036, T12-001)

## Defectos

1. La [verificación de traslados](t19-traslados-verificacion.md) mostró que una función C++ `__vectorcall` con un
   record por valor no se importa: `C/C++ return type is not yet mapped` y `C/C++ parameter 0 is not yet mapped`.
2. Al probar el retorno de un record C++ con un constructor declarado apareció un defecto anterior de T12-001. Una
   plantilla NativeSharp precompilada que llama a `C8 makec(int)` (`cdecl`, 8 bytes) termina con `0xC0000005`. El
   camino directo pasa.

## Causas

1. `projectFunction` (`CppSemanticFunction.cpp`) solo admite records C++ por valor con `cdecl`, `preserve_most`,
   `preserve_none`, `regcall` y `sysv_abi`. Las funciones C `__vectorcall` sí los admiten desde T08-D22: el backend
   clasifica los agregados homogéneos con las dos pasadas de Clang (`vectorCallPassing`). Esa clasificación es
   estructural y no modela las reglas del ABI C++: MicrosoftCXXABI devuelve en memoria un record que no es trivial
   para MSVC, aunque sea un agregado homogéneo.
2. T12-001 guarda en `CppImportedTypeDeclaration::cppIndirectReturn` que Clang devuelve el record en memoria.
   `CppCRecordSnapshot` no tenía ese dato y el catálogo precompilado tampoco lo guardaba. El consumidor lo devolvía
   en `RAX` y la biblioteca escribía en la dirección que recibía en `RCX`.

## Cambio

- `ClangSession.cpp`: una función C++ `__vectorcall` con un record por valor conserva el plan de Clang de su
  declaración exacta (`copyPlan`), como `regcall` y `sysv_abi`.
- `CppSemanticFunction.cpp`: `__vectorcall` admite records C++ triviales por valor.
- `OopLlvmDeclarations.cpp`, `vectorCallPlanAgrees`. Antes de declarar la importación, el backend compara su
  clasificación con el plan de Clang:
  - la convención, el símbolo y el número de parámetros;
  - el puntero oculto del resultado;
  - cada agregado homogéneo, en registros (`inreg`, mismos miembros) o por dirección;
  - cada otro record, directo como entero o por dirección.
  Si algo difiere, la importación se rechaza con el motivo. Una función con un record C++ y sin plan también se
  rechaza.
- `OopLlvmDeclarations.cpp` y `OopLlvmClosuresDelegatesEvents.cpp`: un record con `cppIndirectReturn` vuelve por
  `sret` también con `vectorcall`, en la declaración y en la llamada. Antes la declaración usaba el retorno de la
  función y la llamada solo el tamaño.
- `CppCRecordMetadataCodec.cpp`:
  - el plan de una función C++ `__vectorcall` viaja en el campo opcional de plan que ya usan `regcall`,
    `preserve_none` y las convenciones planificadas. Al leerlo se comprueban la convención, el símbolo y los
    parámetros;
  - los records que Clang devuelve en memoria van en un bloque opcional al final del catálogo de records, con una
    marca por record en su orden. El bloque se escribe solo si alguna marca está activa. Un catálogo sin esos
    records conserva sus bytes, y uno anterior al bloque se lee sin marcas. La marca exige
    `cppValueObservation`, y un bloque sin marcas activas se rechaza por no canónico. `reconstructCImportedTypes`
    la copia a `cppIndirectReturn`.
- `CppSemanticRecord.h` y `CppSemanticRecord.cpp`: `CppCRecordSnapshot::cppIndirectReturn`, tomado de la
  declaración que observó Clang.

## Evidencia

El fixture `pruebas-tanda-19-traslados` tiene cuatro casos nuevos. Cada uno compara con un control de Clang 23.1.0,
O0–O3, en estática y DLL. Las mismas firmas se repiten desde una plantilla NativeSharp precompilada, con el
encabezado C++ visible para el consumidor y sin él. También se compara la forma de cada declaración en el IR de NSC
y en el de Clang: convención, resultado, `inreg`, `sret`, puntero o escalar.

| Caso | Firmas | Resultado |
|---|---|---|
| `vectorcall-hva` | `{double, double}` como parámetro y como resultado | 183 |
| `vectorcall-mixto` | siete argumentos: un `double`; `{float x3}` en registros; tres `{double x4}` por dirección, el último después del sexto argumento; un record de 8 bytes como entero y uno de 12 bytes por dirección; resultados de las cuatro formas | 64273 |
| `vectorcall-constructor` | `{double, double}` con un constructor declarado: en registros como parámetro, por `sret` como resultado | 182 |
| `cdecl-constructor` | record de 8 bytes con constructor declarado: entero como parámetro, `sret` como resultado; el defecto 2 | 365 |

- `logs/fx-vc6.log`: estática, los cinco casos (`tls-campo` incluido).
- `logs/fx-vc7d.log`: DLL. La DLL define `_fltused` porque se enlaza sin el CRT, como el control.
- Regresión dirigida, `lista-vc8.txt` (65 guiones: tanda 8 de callbacks C; tanda 9 de convenciones, `vectorcall` y
  precompilado; tandas 10 a 14 y 19):
  - estática: 62 de 65 (`logs/regresion-vc8`). Los tres guiones de `pruebas-tanda-08-callbacks-c` no llegan a
    compilar: tienen fijas las rutas de un entorno que ya no existe (`A:\nsc-interop-build\tanda1-ON`,
    `A:\al82dev-clang`). Copias locales con este `nsc.exe`, Clang 23.1.0 y NSLink pasan los tres: `vectorcall` C
    (T08-D22) directo en estática y DLL y precompilado con y sin encabezado, O0–O3, con sus tres rechazos;
    callbacks C; identidad de funciones C (`logs/t08-vectorcall-vc8.log`, `t08-callbacks-vc8.log`,
    `t08-identidad-vc8.log`). Los guiones del repositorio no se modificaron;
  - DLL: 41 de 41 (`logs/regresion-dll-vc8d`).
- `lote-ct2`: `ninja -k 0` sin errores. Pasan las nueve CTests de interoperabilidad y
  `nsc_cpp_c_identity_tests`.
- `ctest -R "snapshot|precompiled|record|cpp-interop"`: 21 de 22 (`logs/ctest-vc8.log`). Falla
  `nsc.cpp-interop.session`, que no encuentra los fixtures de sesión (T19-037).

## Sigue pendiente

- Agregados de vectores de Clang de 128 bits (`__m128`, `__m128d`) con `__vectorcall`. NativeSharp ve cada campo
  vector como un arreglo de sus carriles, así que su clasificación no coincide con la de Clang. El plan de Clang
  detecta la diferencia y la importación se rechaza con su motivo.
- Punteros a función C++ `__vectorcall` con records por valor: siguen rechazados.

No se modificó Clang ni NSLink. El contrato NSMeta vigente es el único.
