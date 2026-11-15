# Alcance de la tanda 15

Base: `092a27f0a`. [Auditoría inicial](TANDA-15-AUDITORIA-INICIAL.md).

Cambios, en este orden:

1. Runtime de C++: arranque por el CRT estático de MSVC (`--crt-startup`, con `--link-lib`), encabezados de
   la STL en la sesión y definiciones inline que alcanzan las emitidas, con el pipeline de Clang.
2. Excepciones C++ que atraviesan NativeSharp: limpieza, `noexcept`, excepciones sin atrapar, `catch`
   NativeSharp y `panic` en un callback (T15-001 a T15-007, T15-013).
3. Callbacks NativeSharp llamados por C++, inmediatos, conservados y con contexto (T15-008 a T15-012).
4. Recursos: punteros inteligentes, allocators, tipos movibles no copiables y corutinas (T15-014 a T15-018).
5. Validación agrupada y cierre.

Cada capacidad se compara con un control compilado por Clang. Se prueba en estática y DLL, de O0 a O3.

Fuera: convertir una excepción C++ en una excepción NativeSharp; lanzar excepciones NativeSharp a través de
frames C++.

Presupuesto inicial: ClangSession.cpp, CppLifetimeObjectEmitter.cpp, WindowsRuntimeBuilder.cpp/.h,
WindowsExecutableBuilder.cpp/.h, LldLinker.cpp/.h, CompilerInvocation.cpp/.h, CompilerOptionParser.cpp,
CompilerSession.cpp, DriverExecutionScope.h, DriverArtifactGeneration.cpp, PrecompiledModuleConfiguration.cpp,
fixtures pruebas-tanda-15-* y documentación.
