# Tanda 15: auditoría inicial

Base: `092a27f0a` (cierre de la tanda 14). Clang 23.1.0, objetivo `x86_64-pc-windows-msvc`.

La sonda `pruebas-tanda-15-auditoria/sonda.py` (no es una prueba de soporte) compila cada caso con Clang y
un consumidor NativeSharp. Evidencia: `A:/temp/nsc-t15-sonda-gybat8up`; registro en
`build-claude-t08/logs/sonda-t15.log`. Los experimentos de causa están en `build-claude-t08/tools/exp-*.py`.

## Lo que hace NSC hoy

- **Enlace del runtime de C++.** Una biblioteca que Clang compila con excepciones (lo normal en C++) referencia
  `__CxxFrameHandler3`, `_CxxThrowException` y la vtable de `type_info` en cuanto una función tiene una
  región de limpieza, incluida la de una función `noexcept` que llama a un callback. NSC enlaza solo su
  runtime con `/nodefaultlib`, así que todos los casos con callbacks o excepciones fallan al enlazar.
- **Arranque.** El ejecutable entra por `__nativesharp_start`. Experimento (`exp-crt3.py`): con el runtime
  estático de MSVC enlazado pero sin inicializar, lanzar una excepción o reservar con `operator new` termina
  en una violación de acceso y un constructor global no se ejecuta (devuelve 0 en vez de 7). Con la entrada
  `mainCRTStartup` del CRT, que llama a un `main` que entra en NativeSharp, la excepción atraviesa el
  callback NativeSharp (41), el constructor global corre (7), su destructor corre al salir y `unique_ptr`
  reserva (5).
- **Encabezados de la STL.** Con la STL de MSVC como directorio de sistema, NSC termina con una violación de
  acceso al analizar `<type_traits>`. Bisección (`bisect-tt2.py`): la especialización parcial de una
  plantilla de variable `_Is_specialization_v<_Template<_Types...>, _Template>` es un `VarDecl` del
  namespace y `collectGlobals` le pide valor y nombre decorado como a una variable.
- **Código inline de la STL.** Con `<coroutine>`, LLVM aborta (`Do not know how to promote this operator`):
  `emitCppInlineObject` genera el objeto del IR de Clang sin el pipeline de pases que Clang siempre ejecuta,
  y los intrínsecos `llvm.coro.*` llegan a la selección de instrucciones. Con `<memory>`, el enlace deja sin
  definir funciones inline o instanciadas que llaman las emitidas (`std::swap<int*>`,
  `std::current_exception`): `emitCppInlineIR` solo da al generador de código las pedidas.
- **NSLink** rechaza `libucrt.lib` (`NSL1023`, reglas COMDAT); lld-link la enlaza.
- **Callbacks.** Una lambda sin capturas asignada a un local del tipo puntero a función se convierte con un
  adaptador; pasarla directamente como argumento no resuelve la sobrecarga. Una lambda con capturas se
  rechaza (`extern C lambda must not capture state`).
- **Excepciones NativeSharp.** `catch (Tipo e)` necesita `NativeSharp.Core.Exception`; un proyecto sin la
  biblioteca núcleo no lanza excepciones NativeSharp.
- **Recursos.** Un tipo movible no copiable devuelto por valor no se importa; copiar un `std::unique_ptr` en
  NativeSharp se rechaza (`NS3020`); `std::unique_ptr` no se construye desde NativeSharp porque su constructor
  por defecto es una plantilla.

## Modelo de excepciones del objetivo (T15-002)

Clang 23.1.0 para `x86_64-pc-windows-msvc` usa el modelo de Microsoft: `throw` llama a `_CxxThrowException`,
las funciones con `catch` o limpieza usan la personalidad `__CxxFrameHandler3` con funclets, y el
desenrollado es el de SEH (`RtlUnwindEx`). NSC usa `__C_specific_handler` en sus regiones `try`, también
SEH, así que una excepción C++ desenrolla los frames de NSC y ejecuta su limpieza.

## Plan

Ver [SCOPE-LOCK-TANDA-15.md](SCOPE-LOCK-TANDA-15.md).
