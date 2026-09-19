# Bindings del compilador autoalojado

Los proyectos públicos viven en `lib/NativeSharp.LLVM.C` y
`lib/NativeSharp.Win32.Compiler`; este directorio conserva la decisión arquitectónica y no duplica
fuentes. El nuevo compilador invoca directamente la API C versionada de LLVM: no pasa por un
wrapper C++ perteneciente a NSC.

Cada símbolo importado debe aparecer en `selfhost/contracts/llvm-c-api.contract.json`, tener
ownership documentado y una prueba de carga, firma y versión. Los servicios Win32 se consumen por
la fachada `NativeSharp.Win32.Compiler`; el frontend no importa APIs de Windows por su cuenta.
