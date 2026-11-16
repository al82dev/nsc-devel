# Alcance de la tanda 17

Base: `4f5bb7013`. [Auditoría inicial](TANDA-17-AUDITORIA-INICIAL.md).

Cambios, en este orden:

1. Ubicaciones C++ en los diagnósticos: la declaración del encabezado cuando una importación falla y la
   ubicación de cada nota de Clang (T17-001, T17-002, T17-004).
2. Alternativas de una sobrecarga: una nota por candidato, con su declaración y por qué no acepta la llamada,
   en aridad, tipos y ambigüedad (T17-003).
3. Símbolo ausente frente a ABI distinta: cuando el enlace falla, cada función C/C++ del programa que ninguna
   entrada define se nombra con su declaración y, si la biblioteca define el mismo nombre con otra firma, se
   dice que encabezado y biblioteca discrepan (T17-005, T17-006).
4. Depuración del código C++ que NSC emite: con `-g`, las funciones inline del encabezado llevan CodeView
   (T17-007, T17-008).
5. Caché de compilación para sesiones C++, con la identidad de opciones, preprocesado y todos los archivos
   que carga Clang, y medición antes y después (T17-009 a T17-011).
6. Guía de uso de bibliotecas C/C++ desde un proyecto NativeSharp (T17-012).
7. Validación agrupada y cierre.

Avance: cambio 1 en [t17-ubicaciones.md](registro-commits/t17-ubicaciones.md); cambio 2 en [t17-sobrecargas.md](registro-commits/t17-sobrecargas.md); cambio 3 en [t17-simbolos.md](registro-commits/t17-simbolos.md); cambio 4 en [t17-depuracion.md](registro-commits/t17-depuracion.md); cambio 5 en [t17-cache.md](registro-commits/t17-cache.md) y [t17-cache-pruebas-t09.md](registro-commits/t17-cache-pruebas-t09.md); cambio 6 en [t17-guia.md](registro-commits/t17-guia.md).

Cada capacidad se compara con un control compilado por Clang cuando hay uno (depuración y caché) y se prueba
en estática y DLL, de O0 a O3 donde el nivel importa.

Fuera: ubicaciones C++ como rangos con subrayado del encabezado; prefijos de depuración
(`-fdebug-prefix-map`) en el código C++ emitido.

Presupuesto inicial: CppSessionDiagnostic.h, ClangSession.cpp, CppInteropSession.h, CallAnalyzer.cpp,
NamespaceFunctionSymbol.h, CppSemanticFunction.cpp, ThinLtoBackend.cpp/.h, ProjectOopObjectGenerator.cpp/.h,
DriverArtifactGeneration.cpp, PrecompiledModuleConfiguration.cpp, fixtures pruebas-tanda-17-* y
documentación.

Cerrada el 2026-11-15: [cierre y pendientes](registro-commits/t17-cierre.md); [validación agrupada](TANDA-17-VALIDACION-AGRUPADA.md).
