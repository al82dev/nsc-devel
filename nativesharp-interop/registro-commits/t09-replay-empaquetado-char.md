# T09-003: records C empaquetados y campos `char` en el replay precompilado

## Defecto

Sondas `build-claude-t08/tools/sonda-empaquetado.py` y `sonda-directa.py`. Antes del cambio, con
plantilla precompilada y sin encabezado:

- los punteros `sysv_abi` y `regcall` con un record C empaquetado se rechazaban con
  `<convención> function pointer signature is not yet supported`
  (`A:/temp/nsc-t09-sonda-empaquetado-acri2rao`);
- la llamada directa `sysv_abi` variádica con el record empaquetado `{int; char; int}` se rechazaba
  con `sysv_abi variadic argument type is not yet supported`.

Con encabezado, o en sesión viva, los mismos casos pasaban. Clang compila todos.

Causa en el código:

- El replay de records C del catálogo (`makeSimpleCRecordAbiReplay`, `CppCRecordAbiReplay.h`)
  solo escribía tipos incorporados con signo explícito (`signed char`, `unsigned char`, `int`, …),
  flotantes y `_Bool`. Un campo `char` sin signo explícito no tenía escritura. El replay devolvía
  `nullopt` y el productor (`CppSemanticRecord.cpp`) dejaba en falso las dos marcas de sintaxis
  reconstruible del record: `plainReplaySyntax` y `packedReplaySyntax`. Esto afecta a cualquier
  record C con un campo `char`, empaquetado o no.
- La planificación del alias de puntero precompilado (`planPrecompiledCppFunctionPointerForAlias`,
  `CppProjectSessions.cpp`) exigía `plainReplaySyntax` en el record de la firma y en el record del
  catálogo. Un record empaquetado solo tiene `packedReplaySyntax`, así que se rechazaba aunque el
  replay pudiera reconstruirlo. La llamada variádica ya aceptaba las dos marcas.

## Cambio

- `CppCRecordAbiReplay.h`: un campo `char` de 8 bits, con alineación de 8 bits, se escribe como
  `char`. El replay se compila para el mismo destino, así que conserva el signo del `char` de Clang.
  La comparación posterior, campo por campo, del layout reconstruido con el del catálogo no cambia.
- `CppProjectSessions.cpp`: la planificación del alias acepta un record con `plainReplaySyntax` o
  `packedReplaySyntax`, en la firma y en el catálogo. El replay ya reconstruye un record empaquetado
  con `__attribute__((packed))`. Se mantienen sin cambios las demás comprobaciones:
  - módulo;
  - identidad del record;
  - layout del replay contra el catálogo;
  - plan de Clang contra la firma de origen.

No se modificó Clang ni NSLink. El catálogo no cambia de forma y el contrato NSMeta vigente sigue
siendo el mismo.

## Evidencia

### Sondas tras el cambio

- `A:/temp/nsc-t09-sonda-empaquetado-cxplwn7o`: todos los records empaquetados pasan en punteros
  `sysv_abi` y `regcall` precompilados. Solo `_Alignas` sigue rechazado.
- `A:/temp/nsc-t09-sonda-directa-z2w4dvv8`: la llamada `sysv_abi` variádica con el record
  empaquetado pasa. `_Alignas` sigue rechazado.

### Fixture nuevo

`pruebas-tanda-09-precompilado-empaquetado/validar.py`, con y sin `--dll`.

Records C `T09R`:

| Caso | Campos |
|---|---|
| `empaquetado` | `packed { int x; char c; int z; }` (9 bytes) |
| `empaquetado-volatile` | `packed { int x; char c; volatile int z; }` |
| `empaquetado-corto` | `packed { short a; int x; char c; }` |
| `empaquetado-double` | `packed { char c; double d; int x; }` |
| `char` | `{ char c; int x; char d; }` |
| `char-arreglo` | `{ char s[3]; int x; }` |

Para `sysv_abi` y `regcall`, cada caso, de O0 a O3:

- se prueba con un puntero `T09Ptr (T09R r, int k)` devuelto por la biblioteca;
- un control C compilado por Clang 23.1.0 y el consumidor NativeSharp de la plantilla, con y sin
  encabezado, devuelven el mismo valor;
- la llamada indirecta del consumidor lleva la convención de Clang (`x86_64_sysvcc` o
  `x86_regcallcc`).

Además, para la llamada directa `sysv_abi` variádica `t09_v(T09R r, int k, ...)`, con el record
como argumento fijo y como argumento variable, los argumentos físicos del IR de NSC (tipo,
extensión y `byval`) coinciden con los de la llamada de Clang.

El control negativo usa `{ int x; int y; __unaligned int z; }`. El replay no reconstruye
`__unaligned`, que el catálogo no guarda. Sigue rechazado:

- en los dos punteros, con `<convención> function pointer signature is not yet supported`;
- en la llamada variádica, con `sysv_abi variadic argument type is not yet supported`.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t09-empaquetado-1jwg7t4y` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t09-empaquetado-sxxn0upl` |

Regresión de la tanda: ver [TANDA-09-VALIDACION-AGRUPADA.md](../TANDA-09-VALIDACION-AGRUPADA.md).

## Límites

- `_Alignas` y otros atributos de campo siguen fuera del replay precompilado: el catálogo no los
  guarda.
- `__unaligned` sigue fuera del replay.
- Los records C++ no cambian: no usan este replay.
