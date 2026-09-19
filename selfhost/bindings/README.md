# Bindings del compilador autoalojado

Este árbol contendrá `LLVM.C` y `Win32.Compiler`. El nuevo compilador invocará directamente la API
C versionada de LLVM; no pasará por un wrapper C++ perteneciente a NSC.

Cada símbolo importado deberá aparecer en `llvm-c-api.contract`, tener ownership documentado y una
prueba de carga, firma y versión.
