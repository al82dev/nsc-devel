# T14-001, T14-002 y T14-003: macros C y C++ consumidas desde NativeSharp

## Defecto

En la [auditoría inicial](../TANDA-14-AUDITORIA-INICIAL.md), `#define T14_N 4`, una expresión, un flotante,
una macro C y una macro de función terminaban en `unknown identifier` al usarlas desde NativeSharp.

Causa en el código:

- `ClangSession.cpp` solo recorre declaraciones del AST; una macro es estado del preprocesador
  (`Preprocessor::macros()`) y no llega al inventario de globales;
- `CppGlobalDeclaration` no distingue una constante de macro de una variable;
- la búsqueda de llamadas (`ProjectCompilationSnapshots.cpp`, `CallAnalyzer.cpp`) no sabe que un nombre es
  una macro, así que no puede decir por qué no se importa.

## Diseño

- **Clasificación** (T14-001): se consideran las macros que define el encabezado o los que incluye; no las
  de encabezados del sistema, del compilador, de la línea de órdenes (`Define` del proyecto) ni las que
  empiezan por `__nsc_`. Una macro de objeto con expansión se evalúa; una de función se registra como tal.
- **Valores** (T14-002): la sesión analiza otra vez la misma fuente, con la misma configuración, añadiendo
  `constexpr auto __nsc_macro_X = (X);` por macro (en C, `static const __auto_type`) y
  `-ferror-limit=0`, para que una expansión inválida no impida evaluar las demás. Una variable cuyo valor
  da `VarDecl::evaluateValue` como escalar (entero, enum, bool o flotante de hasta 64 bits) pasa a ser una
  constante global con el tipo que deduce Clang, igual que las constantes de la tanda 13: no tiene
  dirección ni símbolo. El resultado se calcula una vez por sesión.
- **Rechazos** (T14-003): NativeSharp no aplica el preprocesador C a su código. Una llamada o uso de una
  macro de función dice `C/C++ macro 'X' is a function-like macro: NativeSharp does not expand C/C++
  macros; declare an inline function in the header instead`; una macro de objeto que no da un escalar,
  `does not expand to a scalar constant that NativeSharp can import`. Un local o miembro con el mismo
  nombre oculta la macro.

## Cambio

- `CppInteropSession.h`: `CppMacroName` y `macrosSnapshot`.
- `CppGlobalDeclaration.h`: `macro`.
- `ClangSession.cpp`: `evaluateScalarConstant` (compartido con las constantes de la tanda 13),
  `macroConstantsLocked`, `macrosSnapshot`; `globalsSnapshot` añade las constantes de macro.
- `CppSemanticInventory.cpp`: una constante de macro no es almacenamiento interno de C y se proyecta como
  estado constante en C y en C++.
- `ProjectCompilation.cpp/.h`: `cppUnimportedMacros_`, por proyecto, con el motivo.
- `ProjectNamespaceFunctionModel.h`, `ProjectCompilationSnapshots.cpp`, `CallAnalyzer.cpp`: el sitio de
  llamada de una macro no importada y su diagnóstico.

No se modificó Clang ni NSLink. El catálogo NSMeta no cambia de formato.

## Evidencia

Fixture nuevo `pruebas-tanda-14-macros/validar.py`, con y sin `--dll`. Macros de objeto con entero, sin
signo, 64 bits, negativo, expresiones que usan otras macros, flotante `double` y `float`, `true`, un
carácter, un enumerador (`T14::Color::Green`), una llamada a una función `constexpr` y una macro de un
encabezado incluido; resultado 132 frente al control C++ de Clang 23.1.0, O0–O3, con la llamada C++ de NSC
igual a la de Clang y sin ningún símbolo `__nsc_macro_`. Un encabezado C con macros entera, negativa y
flotante se comprueba igual.

Control negativo: `T14_SQUARE(3)` se rechaza con el motivo de macro de función; la contraparte llama a la
función inline equivalente.

| Variante | Resultado | Evidencia |
|---|---|---|
| Estática | pasa (O0–O3) | `A:/temp/nsc-t14-macros-mtkhr9g8` |
| DLL | pasa (O0–O3) | `A:/temp/nsc-t14-macros-2oko4__2` |

En una corrida simultánea con otros cinco fixtures, el control C++ de Clang (no NSC) devolvió 0 en la
variante DLL; repetida sola, pasa.

Regresión: en la validación agrupada de la tanda (`TANDA-14-VALIDACION-AGRUPADA.md`).

## Límites

- Las macros de función no se expanden en código NativeSharp.
- Una macro que se expande a una cadena, un puntero, un record o un tipo no se importa.
- Una macro de la línea de órdenes (`Define`) no se importa como constante: su valor lo escribe el proyecto.
