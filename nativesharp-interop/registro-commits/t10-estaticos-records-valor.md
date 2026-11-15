# T10-005 y T10-013: métodos estáticos C++ con records triviales por valor

## Defecto

Un método estático C++ que toma o devuelve un record por valor no se importaba: `T10.S.grow(c, 2)`
daba `no compatible member named 'grow'`. Era un límite registrado en
[t10-tipos-anidados.md](t10-tipos-anidados.md).

Causa en el código:

- `projectCppSemanticStaticMethod` (`CppSemanticFunction.cpp`) mapeaba la firma sin permitir records
  C++ por valor, a diferencia de una función libre `cdecl` (`projectFunction`).
- `ProjectMemberModel.cpp` solo aceptaba enums como tipos por valor de un método estático
  (`valueTypeIsEnum`).

Un método estático C++ no tiene objeto implícito: en Win64 su convención es la de una función libre
`cdecl`. La llamada ya se emite con los tipos físicos de esa convención, que el fixture compara con
el IR de Clang.

## Cambio

- `CppSemanticFunction.cpp` (`projectCppSemanticStaticMethod`): el retorno y los parámetros se mapean
  con records C++ triviales por valor, como en una función libre `cdecl`.
- `ProjectMemberModel.cpp`: un tipo por valor de un método estático puede ser un enum importado o un
  record C++ trivial (`cppTrivialValueEligible`).

Un record con constructor de copia, destructor u otra operación no trivial sigue sin pasar por valor.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Fixture nuevo `pruebas-tanda-10-estaticos-valor/validar.py`, con y sin `--dll`. `struct S` en
`namespace T10` con métodos estáticos que toman o devuelven records de 4, 8, 12, 16 (`double`) y
24 bytes (arreglo `int[6]`). De O0 a O3, el fixture:

- compara un control C++ compilado por Clang 23.1.0 con el consumidor NativeSharp (resultado 54);
- exige las 6 declaraciones del IR de NSC con los tipos físicos de Clang: `i32` e `i64` para 4 y 8
  bytes, puntero para 12, 16 y 24 bytes y `sret` para esos retornos;
- consume una plantilla precompilada, con y sin encabezado.

Control negativo: `S::take(C)`, con `C` con constructor de copia propio, no se importa
(`no compatible member named 'take'`). La contraparte `S::keep(T)`, con un record trivial del mismo
encabezado, pasa.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-estaticos-valor-z84ww6v0` |
| DLL | pasa (O0–O3, con y sin encabezado) | `A:/temp/nsc-t10-estaticos-valor-3nl46qkl` |

Regresión de las tandas 9 y 10: ver [TANDA-10-VALIDACION-AGRUPADA.md](../TANDA-10-VALIDACION-AGRUPADA.md).

## Límites

- Records no triviales por valor: tanda 11 (T11-015).
- Referencias en métodos estáticos: no se proyectan todavía (las funciones libres sí,
  [t10-referencias-lvalue.md](t10-referencias-lvalue.md)).
