# T11-007 y T11-013: destructores noexcept implícitos y miembros especiales en línea

## Defecto

En la [auditoría inicial](../TANDA-11-AUDITORIA-INICIAL.md), un local de un record con
`D(int) noexcept` y `~D();` se rechazaba con `C++ local lifetime currently requires a non-throwing
constructor and destructor`. En C++ un destructor declarado sin especificación de excepciones es
`noexcept` de forma implícita.

Causa en el código:

- `ClangSession.cpp` (`collectRecords`) solo tomaba la especificación de excepciones ya resuelta. La
  de un destructor declarado sin `noexcept`, o de un miembro especial por defecto, queda sin evaluar
  (`EST_Unevaluated`) hasta que Sema la calcula, y NSC la trataba como lanzadora.
- Al resolverla aparecieron dos defectos más con `~E() = default;`:
  - un constructor o destructor definido en la clase, o por defecto en su primera declaración, es
    `inline` y la biblioteca no lo exporta, pero `emitCppInlineIR` excluía los constructores y
    destructores (límite de T10-012);
  - un destructor trivial no tiene efecto y Clang no lo llama, pero NSC sí: en DLL pedía
    `__imp_??1E@T11@@QEAA@XZ`, que la biblioteca no exporta.

## Cambio

- `ClangSession.cpp`:
  - `collectRecords` recibe Sema y resuelve con `ResolveExceptionSpec` la especificación sin evaluar
    de cada método, como ya se hacía con los miembros implícitos;
  - guarda la trivialidad de cada método (`CppMethodDeclaration::isTrivial`);
  - `emitCppInlineIR` emite también constructores y destructores definidos en la clase o por defecto
    en su primera declaración, con su variante de objeto completo (en MS, la base para el
    destructor). Un miembro por defecto se define antes con Sema (`DefineImplicitDestructor`,
    `DefineImplicitDefaultConstructor`, de copia o de movimiento).
- `CppSemanticFunction.cpp` y `.h`: el método de vida proyectado conserva la trivialidad, que llega
  al miembro (`cppLifetimeTrivial`).
- `OopLlvmCleanup.cpp`: la destrucción de un local C++ no llama a un destructor trivial.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Fixture nuevo `pruebas-tanda-11-destructores/validar.py`, con y sin `--dll`. Encabezado en
`namespace T11`:

- `D` con `D(int) noexcept` y `~D();`, que anota su valor en un registro global;
- `E` con `E(int) noexcept` y `~E() = default;` (trivial);
- `H` con un miembro `D` y destructor implícito.

El consumidor construye dos `D` en un bloque, un `E` y un `H` en otros; al salir de cada bloque se
destruyen en orden inverso (registro `214`). De O0 a O3, el fixture compara un control C++ compilado
por Clang 23.1.0 con el consumidor NativeSharp (resultado 226) y exige las 6 declaraciones del IR de
NSC con los tipos físicos de Clang.

Control negativo: un destructor `noexcept(false)` sigue rechazado con el diagnóstico de vida no
lanzadora. La contraparte `~K();` del mismo encabezado se acepta.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t11-destructores-sfwibsji` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t11-destructores-_d8vra5v` |

Regresión: ver [TANDA-11-VALIDACION-AGRUPADA.md](../TANDA-11-VALIDACION-AGRUPADA.md).

## Límites

- Un constructor que puede lanzar sigue rechazado en un local: la limpieza durante el desenrollado es
  de la tanda 15.
- Los objetos C++ con vida no trivial no se transportan todavía en una plantilla precompilada
  (`C++ record has no eligible value observation`).
