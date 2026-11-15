# Tanda 14: auditoría inicial

Base: `7b4994e2e` (cierre de la tanda 13). Clang 23.1.0, objetivo `x86_64-pc-windows-msvc`.

La sonda `pruebas-tanda-14-auditoria/sonda.py` (no es una prueba de soporte) compila cada caso con
Clang y un consumidor NativeSharp que devuelve 0 si los valores coinciden. Evidencia de la corrida:
`A:/temp/nsc-t14-sonda-c3xjk8mj`; registro en `build-claude-t08/logs/sonda-t14.log`.

## Lo que hace NSC hoy

- La sesión de Clang analiza el encabezado con la configuración del proyecto (`Define`, `Undefine`,
  rutas, estándar). El preprocesador de Clang ya decide las condiciones: el caso
  `condicion-configuracion` pasa.
- Las macros no se importan. `#define T14_N 4`, una expresión, un flotante, una macro C y una macro de
  función terminan en `unknown identifier`, sin decir que el nombre es una macro.
- Los bitfields de C se leen y escriben con el layout de Clang (`bitfield-c` pasa); los de C++ se
  excluyen del record (`no compatible member named 'a'`), porque la importación de campos los limita a
  records C.
- Un campo de un record `[[gnu::packed]]` con alineación menor que la natural se excluye en C++
  (`no compatible member named 'b'`). `alignas` pasa.
- Una función con un puntero a miembro no se importa y la llamada calificada dice `unknown identifier`
  del namespace. `describeType` no distingue el puntero a miembro de otro tipo sin soporte.
- `[[no_unique_address]]` no tiene efecto en el ABI de Microsoft y la sonda esperaba lo contrario; el
  caso devuelve 1 porque su expectativa era errónea. El atributo que sí cambia el layout en este objetivo
  es `[[msvc::no_unique_address]]`.
- Un encabezado que importa un módulo C++20 lo rechaza Clang (`module 'std' not found`): la sesión no
  recibe interfaces de módulo compiladas.
- Un cambio en un encabezado incluido indirectamente invalida la caché: `dependencia-transitiva`
  devuelve 3 y, tras el cambio, 5.

## Interfaces de Clang

- `Preprocessor::macros()` y `MacroInfo` para clasificar macros: de objeto o de función, cuerpo vacío.
- La evaluación de una macro de objeto se hace analizando una declaración
  `constexpr auto __nsc_macro_X = (X);` (en C, `static const __auto_type`) en una unidad aparte con la
  misma configuración; el valor lo da el evaluador de constantes de Clang (`VarDecl::evaluateValue`), no NSC.
- `ASTRecordLayout` da el offset en bits de cada bitfield; `FieldDecl::getBitWidthValue`.
- `Type::isMemberPointerType` distingue los punteros a miembro.

## Plan

Ver [SCOPE-LOCK-TANDA-14.md](SCOPE-LOCK-TANDA-14.md).
