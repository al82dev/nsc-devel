# T09-003: enums de base `wchar_t` con `swiftcall` y `swiftasynccall`

## Defecto

Un enum C++ de base `wchar_t` por valor se proyectaba con `regcall`, `sysv_abi` y `preserve_*`,
pero se rechazaba con `swiftcall` y `swiftasynccall`:

- `C/C++ calling convention 'swiftcall' is not yet supported in HIR`;
- límite registrado en [t09-enum-cpp-convenciones.md](t09-enum-cpp-convenciones.md).

El fixture de enums C++ excluía ese enum con las dos convenciones Swift. Clang compila los dos
casos y pasa el enum como su entero de 16 bits sin signo.

Causa en el código. Las dos convenciones Swift usan el validador de firmas escalares planificadas
(`isCppPlannedScalarConventionPlan`). Ese validador compara cada tipo con
`regCallFloatRecordScalarMatches` (`CppRegCallAbiValidation.h`). La rama del enum de esa función
compara la base con su grafía canónica, y la lista solo conocía `unsigned short`, no `wchar_t`.
Las otras convenciones no pasan por esa función: leen la clave del entero subyacente
(`cppEnumAbiScalarKey`), que ya aceptaba `wchar_t`.

## Cambio

- En la rama del enum de `regCallFloatRecordScalarMatches`, una base `wchar_t` sin signo, de
  16 bits de tamaño y de alineación, se compara con un entero físico de 16 bits.
- `wchar_t` se acepta solo como base de un enum. Un parámetro `wchar_t` suelto no cambia.
- Las comprobaciones previas de la rama no cambian: identidad y USR del enum, base sin
  calificar, tamaño y alineación iguales a los de la base.
- En el fixture `pruebas-tanda-09-cpp-enum-convenciones/validar.py`, la firma `wide` se prueba
  también con `swiftcall` y `swiftasynccall`.

No se modificó Clang ni NSLink. El contrato NSMeta vigente no cambia.

## Evidencia

El fixture exige en el IR de NSC:

- la convención, y las extensiones de cada parámetro y del retorno, iguales a las de Clang;
- esa convención en cada llamada directa.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática + precompilado, `swiftcall` y `swiftasynccall` | pasa (O0–O3) | `A:/temp/nsc-t09-cpp-enum-convenciones-f0u3d9ml` |
| DLL + precompilado, `swiftcall` y `swiftasynccall` | pasa (O0–O3) | `A:/temp/nsc-t09-cpp-enum-convenciones-7jfi9jai` |
| Estática + precompilado + `--espacios`, `swiftcall` y `swiftasynccall` | pasa (O0–O3) | `A:/temp/nsc-t09-cpp-enum-convenciones-ns-10gp3mks` |

Regresión del subconjunto afectado: ver [TANDA-09-VALIDACION-AGRUPADA.md](../TANDA-09-VALIDACION-AGRUPADA.md).

## Límites

- Los punteros `swiftcall` y `swiftasynccall` siguen sin tipo de función NativeSharp. Este
  cambio solo cubre las llamadas directas.
