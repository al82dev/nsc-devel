# Alcance de la tanda 16

Base: `c42a52c74`. [Auditoría inicial](TANDA-16-AUDITORIA-INICIAL.md).

Cambios, en este orden:

1. Inicializadores dinámicos sin arrancar: un ejecutable o una DLL NativeSharp que toma, sin
   `--crt-startup`, un miembro de una biblioteca estática (o un objeto emitido) con `.CRT$XCU` o `.CRT$XIU`
   se rechaza nombrando la entrada y el miembro (T16-003).
2. Arranque, constructores y destructores globales, orden de inicialización, DLL y `DllMain` (T16-001 a
   T16-005, T16-008, T16-012, T16-013).
3. TLS con inicialización dinámica desde más de un hilo (T16-006, T16-007).
4. Enlace separado, símbolos duplicados y LTO (T16-014 a T16-016).
5. Operaciones atómicas compartidas entre hilos (T16-017).
6. Políticas de varias DLL, propiedad de memoria y recursos del CRT (T16-009 a T16-011).
7. Validación agrupada y cierre.

Cada capacidad se compara con un control compilado por Clang. Se prueba en estática y DLL, de O0 a O3.

Fuera: ejecutar los inicializadores de `.CRT$XCU` desde la entrada de NativeSharp sin el CRT; el runtime
dinámico de MSVC (`/MD`); cargar una DLL con `LoadLibrary` desde NativeSharp.

Presupuesto inicial: ThinLtoBackend.cpp/.h, ProjectOopObjectGenerator.cpp/.h, DriverArtifactGeneration.cpp,
fixtures pruebas-tanda-16-* y documentación.
