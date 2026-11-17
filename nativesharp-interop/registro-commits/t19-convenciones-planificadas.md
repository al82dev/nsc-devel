# T19-028: `swiftcall`, `swiftasynccall` e `intel_ocl_bicc` por puntero y con records por valor

Fixture nuevo, con y sin `--dll`: `pruebas-tanda-19-convenciones-planificadas/validar.py`.

- C con las tres convenciones.
- C++ con `swiftcall` y `swiftasynccall`. Clang 23.1.0 no decora `intel_ocl_bicc` en C++, y eso sigue en T19-032.

Sonda de desarrollo: `tools/t19c/d16/sonda-t19-028.py` (`logs/sonda28.log`).

## Defectos

Confirmados con la sonda (`logs/sonda28.log`, antes del cambio):

- NativeSharp no tenía tipos de función con estas convenciones.
  - Una función C que devuelve un puntero de ellas no se importaba: «C/C++ return type is not yet mapped».
  - Un alias de puntero se rechazaba: «unsupported C callback calling convention».
- Una función con un record por valor se rechazaba en HIR: «C/C++ calling convention 'swiftcall' is not yet supported
  in HIR». Las funciones solo escalares ya se admitían desde T09-003.
- Una variádica `intel_ocl_bicc` no se proyectaba («variadic function calling convention is not yet projected»).

## Decisiones

- **Lenguaje**: los tipos de función NativeSharp aceptan las convenciones `swiftcall`, `swiftasynccall` e
  `intel_ocl_bicc`, por ejemplo `fn* swiftcall(int, int) -> int`.
  - Las convenciones LLVM son `swiftcc` (16), `swifttailcc` (20) e `intel_ocl_bicc` (77).
  - Los valores del enum `CallingConvention` se agregan al final, así que los anteriores no cambian.
- **Por puntero**: los escalares viajan como se declaran, igual que en las llamadas directas. Un record Swift por
  puntero queda fuera, porque Clang lo expande y un puntero no tiene declaración con plan.
- **Conversión**: una función C/C++ se convierte a un puntero de su misma convención. Una lambda o un método no se
  convierten.
- **Records `swiftcall` y `swiftasynccall`**: Clang los baja con *coerce-and-expand*:
  - cada elemento sin relleno del struct de coerción es un valor físico aparte, en un offset del record;
  - si no caben en cuatro registros, van por la dirección de una copia o por `sret`;
  - el resultado es un struct de piezas, una pieza suelta o `sret`.
  - El plan de Clang de cada declaración representa ahora *coerce-and-expand* como `Expand`, con sus offsets.
  - NSC declara la función con los tipos físicos del plan, carga cada pieza del record en su offset y reconstruye el
    resultado (`isSwiftRecordPlan`, `CppSwiftRecordAbi.h`).
- **Records `intel_ocl_bicc`**: Clang usa la forma Win64. Un record de 1, 2, 4 u 8 bytes va directo como entero y
  cualquier otro va por copia y `sret`. NSC lo baja por su ruta Win64 de records con la convención del plan
  (`isIntelOclWin64RecordPlan`).
- El plan también viaja en el catálogo precompilado, comprobado por el mismo validador.
- El validador escalar acepta `char` como el entero de 8 bits con signo que es en este destino. Antes solo aceptaba
  `signed char` y `unsigned char`, así que una firma con `char` se rechazaba en todas las convenciones planificadas.

## Qué funciona, como en Clang

Con las tres convenciones en C, y las dos Swift en C++ (en un namespace):

- llamada por un puntero obtenido de C (`getp()`, `getq()` con `double`, `float` y `long long`);
- conversión de una función C/C++ de la misma convención a `fn* swiftcall(int, int) -> int`;
- records por valor en parámetro y en resultado:
  - `R {int; float; double}` (piezas `i32`, `float`, `double`);
  - `S {char; short}` (una pieza `i32`);
  - `B {int[8]}` (cuatro piezas `i64`);
  - `G {int[10]}` (por copia y `sret`).

Todo O0–O3, en biblioteca estática y DLL, directo y desde una plantilla precompilada con y sin encabezado.

En C++ los records vienen de funciones de la biblioteca (`mkr`, `mks`). El cuerpo de una plantilla precompilada
todavía no crea un record C++ con `new`, y eso sigue en T19-016.

## Evidencia

| Fixture | Estática | DLL |
|---|---|---|
| `pruebas-tanda-19-convenciones-planificadas` | `A:/temp/nsc-t19-convenciones-hzq0ssg6` | `A:/temp/nsc-t19-convenciones-dnehsmpw` |

Resultados del control:

- C: 252763, con las tres convenciones;
- C++: 215042.

Rechazo genuino, también en Clang: una función variádica `swiftcall` («variadic function cannot use swiftcall calling
convention», el mismo diagnóstico de Clang en NSC).

`pruebas-tanda-09-convenciones-planificadas`: su rechazo de un record `swiftcall` por valor era obsoleto. Ahora es una
comparación positiva con Clang (12), y queda el rechazo genuino de una `swiftcall` variádica, que Clang también
rechaza. Pasa con `--precompilado` en estática (`A:/temp/nsc-t09-convenciones-planificadas-t9cvwd1g`) y en DLL
(`A:/temp/nsc-t09-convenciones-planificadas-u5002wzp`).

Validación puntual, por decisión del usuario:

- Tanda 9 en estática: pasan 1093 de 1097 (`logs/regresion-d59.log`).
  - Fallan, como en la línea base, los dos `cpp-variadica-preserve-none` (T19-032) y `cpp-tamano-objeto` bajo carga
    (T19-026).
  - `convenciones-planificadas` falló en esa corrida con su negativo obsoleto; convertido, pasa (arriba).
- Tanda 9 en DLL y el fixture nuevo: pasan 18 de 19 (`logs/regresion-dll-d59.log`); el que falla es el mismo negativo
  obsoleto, que convertido pasa.
- Esta validación ejercita también el cambio de `char` en el validador escalar sobre `regcall`, `preserve_*`, `sysv`
  y las uniones.

La regresión global queda para el final.

## Límites

- **Variádica `intel_ocl_bicc`**: Clang la emite, pero el verificador de LLVM 23.1 rechaza esa convención con
  varargs («Calling convention does not support varargs or perfect forwarding!»).
  - El Clang oficial omite el verificador en su compilación de distribución; NSC lo ejecuta.
  - NSC la sigue rechazando con diagnóstico («variadic function calling convention is not yet projected»).
  - Pasa a T19-032, con los demás bloqueos de la cadena de Clang, porque no se modifica Clang ni LLVM.
- Un record Swift por puntero a función y la conversión de una lambda o un método a estos tipos de puntero siguen
  rechazados con diagnóstico. La lambda queda con T19-033.
