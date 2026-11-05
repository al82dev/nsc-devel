# T04-018: evidencia de la acción frontend

`ClangSessionAction.cpp` crea `SessionFrontendAction` y la entrega a la API de
Clang que retiene el AST. La construcción incremental y la batería de sesión
pasaron tras este cambio, como se detalla en
[tanda 04](../../nativesharp-interop/TANDA-04-SESION.md).
