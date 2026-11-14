# T09-003: matriz de convenciones por clase de firma (C, Windows x64)

## Objetivo

Medir si cada convención que NSC conoce conserva el símbolo y la convención de Clang
en cada clase de firma C, no solo en `int(int,int)`. Las siete convenciones son
`cdecl`, `vectorcall`, `regcall`, `sysv_abi`, `preserve_most`, `preserve_all` y
`preserve_none`. Las 18 clases de firma son:

- los once escalares (`i8`/`u8`/`i16`/`u16`/`i32`/`u32`/`i64`/`u64`, `bool`, `float`,
  `double`), de ida y vuelta;
- un puntero que se escribe y se devuelve;
- los records `{int,int}` (8 bytes), `{int,int,int}` (12), `{int[9]}` (36),
  `{float,float}` (8) y `{double,int}` (16), modificados y devueltos;
- una firma mixta `double(float, int*, R8, double)`.

Cada una se llama directamente y por un puntero a función devuelto por C, con
biblioteca estática y O0.

## Método

`pruebas-tanda-09-convenciones/validar-matriz-firmas.py` compila cada caso con Clang.
Si NSC acepta la llamada, exige la convención LLVM de Clang en la declaración o en la
llamada indirecta, y que el ejecutable compruebe los valores devueltos y que el
argumento original no cambie. Cualquier aceptación con otra convención o con un
resultado incorrecto hace fallar la prueba. Los rechazos se registran.

## Resultado

Evidencia: `A:/temp/nsc-t09-matriz-firmas-ousxe__8` (`matriz.md` completa). Hay 252
celdas: 242 pasan, 10 se rechazan con diagnóstico y ninguna se acepta de forma
incorrecta.

Celdas rechazadas, que son capacidades pendientes de T09-003:

| regcall | mixta | rechazo | nsc: error: regcall currently supports only int scalar signatures or other integer types, bool, float, double, void and pointers for __regcall3__t09_f | rechazo | nsc: error: regcall function pointer signature is not yet supported |
| sysv_abi | record8 | pasa |  | rechazo | nsc: error: Stage 34 local initialization failed: sysv_abi C++ function pointer record provenance is invalid |
| sysv_abi | record36 | pasa |  | rechazo | nsc: error: Stage 34 local initialization failed: sysv_abi function pointer record return is not supported |
| sysv_abi | record-flotante | pasa |  | rechazo | nsc: error: Stage 34 local initialization failed: sysv_abi function pointer record return is not supported |
| sysv_abi | record-mixto | pasa |  | rechazo | nsc: error: Stage 34 local initialization failed: sysv_abi function pointer record return is not supported |
| sysv_abi | mixta | pasa |  | rechazo | nsc: error: sysv_abi function pointer signature is not yet supported |
| preserve_none | record36 | pasa |  | rechazo | nsc: error: Stage 34 local initialization failed: preserve_none function pointer signature is not yet supported |
| preserve_none | mixta | rechazo | nsc: error: preserve_none does not yet support this signature for t09_f | rechazo | nsc: error: preserve_none function pointer signature is not yet supported |

## Lectura

- `cdecl`, `vectorcall`, `preserve_most` y `preserve_all` pasan todas las clases,
  directas y por puntero.
- `regcall` pasa todo salvo la firma mixta, que combina escalares y un record.
- `sysv_abi` pasa todo en llamada directa. Por puntero rechaza cinco clases con record:
  la de 8 bytes enteros por un error de procedencia que menciona C++, los retornos de
  record de 36, 8 flotante y 16 bytes, y la firma mixta.
- `preserve_none` en C rechaza la firma mixta, directa y por puntero, y el puntero con
  record de 36 bytes.

Esta matriz no sustituye las pruebas en DLL y precompiladas de cada convención, que
siguen en sus fixtures. T09-003 sigue abierto mientras existan estas 10 celdas.
