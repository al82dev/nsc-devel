# Alcance de la tanda 14

Base: `7b4994e2e`. [Auditoría inicial](TANDA-14-AUDITORIA-INICIAL.md).

Cambios, en este orden:

1. Macros de objeto que Clang evalúa a un escalar se importan como constantes; las de función y las que
   no dan un escalar se rechazan con su motivo al usarlas (T14-001, T14-002, T14-003).
2. Configuración y caché: `Define`, `Undefine` y cambios en encabezados incluidos, con `--artifact-cache`,
   comparados en cada paso con un control de Clang (T14-004, T14-005, T14-011).
3. Registros: bitfields C++, `[[gnu::packed]]`, `#pragma pack`, `alignas`, `__declspec(align)` y
   `[[msvc::no_unique_address]]` con el layout de Clang (T14-006, T14-008).
4. Declaraciones especiales: punteros a miembro rechazados con su motivo; bibliotecas compiladas con
   header precompilado; encabezados que importan módulos C++ (T14-007, T14-009, T14-010).
5. Validación agrupada y cierre.

Avance: cambio 1 en [t14-macros.md](registro-commits/t14-macros.md); cambio 2 en [t14-configuracion.md](registro-commits/t14-configuracion.md); cambio 3 en [t14-registros.md](registro-commits/t14-registros.md); cambio 4 en [t14-especiales.md](registro-commits/t14-especiales.md).

Cada capacidad se compara con un control compilado por Clang. Se prueba en estática y DLL, de O0 a O3.

Fuera: expandir macros de función en código NativeSharp; leer archivos `.pch` o `.pcm` desde la sesión.

Presupuesto inicial: ClangSession.cpp, CppInteropSession.h, CppType.h, CppGlobalDeclaration.h,
CppSemanticInventory.cpp, CppSemanticRecord.cpp, CppSemanticFunction.cpp, ProjectCompilation.cpp,
ProjectCompilationSnapshots.cpp, ProjectNamespaceFunctionModel.h, CallAnalyzer.cpp, fixtures
pruebas-tanda-14-* y documentación.
